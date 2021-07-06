// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/benchmarks/aggregate_throughput_options.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "google/cloud/testing_util/timer.h"
#include "absl/time/time.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

namespace {
using ::google::cloud::storage_experimental::DefaultGrpcClient;
using ::google::cloud::testing_util::CpuAccounting;
using ::google::cloud::testing_util::FormatSize;
using ::google::cloud::testing_util::Timer;
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;
using gcs_bm::AggregateThroughputOptions;
using gcs_bm::ApiName;

char const kDescription[] = R"""(A benchmark for aggregated throughput.

This program is used to evaluate the combined performance of GCS (the service)
and the C++ client library for GCS. It is not recommended as a benchmark for
the client library, as it introduces too many sources of performance variation.
It is useful when the C++ client library team collaborates with the GCS team to
measure changes in the service's performance.

The program measures the observed download throughput given a fixed "dataset",
that is, a collection of GCS objects contained in the same bucket, with a common
prefix. If needed, synthetic datasets can be created using the
`create_dataset.cc` in this directory. Given a dataset and some configuration
parameters the program will:

- Read the list of available objects in the dataset.
- Launch `thread-count` *download threads*, see below for their description.
- Every `reporting-interval` seconds the main thread will report the current
  wall time and the total number of bytes downloaded by all the download
  threads.
- After `running-time` seconds the main thread ask all other threads to
  terminate.
- The main thread then waits for all the "download threads", collects any
  metrics they choose to report, prints these metrics, and then exits.

While running, each download thread performs the following loop:

1) Create a gcs::Client object, see the `AggregateThroughputOptions` struct for
   details about how this client object can be configured.
2) Check if the program is shutting down. If so, just return some metrics.
3) Pick one of the objects in the dataset at random.
4) If so configured, pick a random portion of the object to download, otherwise
   simply download the full object.
5) Download the object, requesting `read-buffer-size` bytes from the client
   library at a time.
6) Once the requested buffer is received, increment a per-thread counter with
   the number of bytes received so far.
7) Keep track of the number of files downloaded, the gRPC peer used to download
   the object (if applicable) and other counters.
8) Go back to step (2) in this list.
)""";

google::cloud::StatusOr<AggregateThroughputOptions> ParseArgs(int argc,
                                                              char* argv[]);

struct ThreadConfig {
  std::atomic<std::int64_t> bytes_received{0};
  std::seed_seq::result_type seed;
};

using Counters = std::map<std::string, std::int64_t>;

Counters RunThread(gcs::Client client,
                   AggregateThroughputOptions const& options,
                   std::vector<gcs::ObjectMetadata> const& objects,
                   ThreadConfig& config);

template <typename Rep, typename Period>
std::string FormatBandwidthGbPerSecond(
    std::uintmax_t bytes, std::chrono::duration<Rep, Period> elapsed) {
  using ns = ::std::chrono::nanoseconds;
  auto const elapsed_ns = std::chrono::duration_cast<ns>(elapsed);
  if (elapsed_ns == ns(0)) return "NaN";

  auto const bandwidth =
      8 * static_cast<double>(bytes) / static_cast<double>(elapsed_ns.count());
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << bandwidth;
  return std::move(os).str();
}

template <typename Rep, typename Period>
std::string FormatBandwidthGiBPerSecond(
    std::uintmax_t bytes, std::chrono::duration<Rep, Period> elapsed) {
  using ::std::chrono::seconds;
  auto const elapsed_s = std::chrono::duration_cast<seconds>(elapsed);
  if (elapsed_s == seconds(0)) return "NaN";

  auto const bandwidth = static_cast<double>(bytes) / gcs_bm::kGiB /
                         static_cast<double>(elapsed_s.count());
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << bandwidth;
  return std::move(os).str();
}

}  // namespace

int main(int argc, char* argv[]) {
  auto options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }
  if (options->exit_after_parse) return 1;

  auto client = gcs::Client();
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (options->api == ApiName::kApiGrpc) {
    auto channels = options->grpc_channel_count;
    if (channels == 0) channels = (std::max)(options->thread_count / 4, 4);
    client = DefaultGrpcClient(
        google::cloud::Options{}.set<google::cloud::GrpcNumChannelsOption>(
            channels));
  }
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  std::vector<gcs::ObjectMetadata> objects;
  std::uint64_t dataset_size = 0;
  for (auto& o : client.ListObjects(options->bucket_name,
                                    gcs::Prefix(options->object_prefix))) {
    if (!o) break;
    dataset_size += o->size();
    objects.push_back(*std::move(o));
  }
  if (objects.empty()) {
    std::cerr << "No objects found in bucket " << options->bucket_name
              << " starting with prefix " << options->object_prefix << "\n"
              << "Cannot run the benchmark with an empty dataset\n";
    return 1;
  }

  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  auto current_time = [] {
    auto constexpr kFormat = "%E4Y-%m-%dT%H:%M:%E*SZ";
    auto const t = absl::FromChrono(std::chrono::system_clock::now());
    return absl::FormatTime(kFormat, t, absl::UTCTimeZone());
  };

  std::cout << "# Start time: " << current_time()
            << "\n# Bucket Name: " << options->bucket_name
            << "\n# Object Prefix: " << options->object_prefix
            << "\n# Thread Count: " << options->thread_count
            << "\n# Reporting Interval: "
            << absl::FromChrono(options->reporting_interval)
            << "\n# Running Time: " << absl::FromChrono(options->running_time)
            << "\n# Read Size: " << options->read_size
            << "\n# Read Buffer Size: " << options->read_buffer_size
            << "\n# API: " << gcs_bm::ToString(options->api)
            << "\n# gRPC Channel Count: " << options->grpc_channel_count
            << "\n# Build Info: " << notes
            << "\n# Object Count: " << objects.size()
            << "\n# Dataset size: " << FormatSize(dataset_size) << std::endl;

  auto configs = [](std::size_t count) {
    std::random_device rd;
    std::vector<std::seed_seq::result_type> seeds(count);
    std::seed_seq({rd(), rd(), rd()}).generate(seeds.begin(), seeds.end());

    std::vector<ThreadConfig> config(seeds.size());
    for (std::size_t i = 0; i != config.size(); ++i) config[i].seed = seeds[i];
    return config;
  }(options->thread_count);

  std::cout << "CurrentTime,BytesReceived,CpuTimeMicroseconds\n"
            << current_time() << ",0,0\n";

  std::vector<std::future<Counters>> tasks(configs.size());
  std::transform(configs.begin(), configs.end(), tasks.begin(),
                 [&](ThreadConfig& c) {
                   return std::async(std::launch::async, RunThread, client,
                                     *options, objects, std::ref(c));
                 });

  auto accumulate_bytes_received = [&] {
    std::int64_t bytes_received = 0;
    for (auto const& t : configs) bytes_received += t.bytes_received.load();
    return bytes_received;
  };

  using clock = std::chrono::steady_clock;
  auto const start = clock::now();
  auto const deadline = clock::now() + options->running_time;
  auto timer = Timer{CpuAccounting::kPerProcess};
  while (clock::now() < deadline) {
    std::this_thread::sleep_for(options->reporting_interval);
    auto const usage = timer.Sample();
    std::cout << current_time() << "," << accumulate_bytes_received() << ","
              << usage.cpu_time.count() << std::endl;
  }
  auto const elapsed = clock::now() - start;
  auto const bytes_received = accumulate_bytes_received();
  std::cout << "# Bytes Received: " << FormatSize(bytes_received)
            << "\n# Elapsed Time: " << absl::FromChrono(elapsed)
            << "\n# Bandwidth: "
            << FormatBandwidthGbPerSecond(bytes_received, elapsed) << "Gbit/s  "
            << FormatBandwidthGiBPerSecond(bytes_received, elapsed)
            << "GiB/s\n";

  Counters accumulated;
  for (auto& t : tasks) {
    auto counters = t.get();
    for (auto const& kv : counters) accumulated[kv.first] += kv.second;
  }
  for (auto& kv : accumulated) {
    std::cout << "# counter " << kv.first << ": " << kv.second << "\n";
  }
  return 0;
}

namespace {

Counters RunThread(gcs::Client client,
                   AggregateThroughputOptions const& options,
                   std::vector<gcs::ObjectMetadata> const& objects,
                   ThreadConfig& config) {
  using clock = std::chrono::steady_clock;
  auto const deadline = clock::now() + options.running_time;
  auto generator = std::mt19937_64(config.seed);
  auto index =
      std::uniform_int_distribution<std::size_t>(0, objects.size() - 1);
  std::vector<char> buffer(options.read_buffer_size);
  auto const buffer_size = static_cast<std::streamsize>(buffer.size());
  Counters counters{{"download-count", 0}};
  // Using IfGenerationNotMatch(0) triggers JSON, as this feature is not
  // supported by XML.  Using IfGenerationNotMatch() -- without a value -- has
  // no effect.
  auto xml_hack = options.api == ApiName::kApiJson
                      ? gcs::IfGenerationNotMatch(0)
                      : gcs::IfGenerationNotMatch();
  while (clock::now() < deadline) {
    auto const& object = objects[index(generator)];
    auto const object_size = static_cast<std::int64_t>(object.size());
    auto range = gcs::ReadRange();
    if (options.read_size != 0 && options.read_size < object_size) {
      auto start = std::uniform_int_distribution<std::int64_t>(
          0, object_size - options.read_size);
      range = gcs::ReadRange(start(generator), options.read_size);
    }
    auto stream = client.ReadObject(object.bucket(), object.name(),
                                    gcs::Generation(object.generation()), range,
                                    xml_hack);
    while (stream.read(buffer.data(), buffer_size)) {
      config.bytes_received += stream.gcount();
    }
    stream.Close();
    ++counters["download-count"];
    auto peer = stream.headers().find(":grpc-context-peer");
    if (peer != stream.headers().end()) {
      ++counters[peer->first + "/" + peer->second];
    }
  }
  return counters;
}

using ::google::cloud::internal::GetEnv;

google::cloud::StatusOr<AggregateThroughputOptions> SelfTest(
    char const* argv0) {
  using google::cloud::internal::Sample;

  for (auto const& var : {"GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME"}) {
    auto const value = GetEnv(var).value_or("");
    if (!value.empty()) continue;
    std::ostringstream os;
    os << "The environment variable " << var << " is not set or empty";
    return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                 std::move(os).str());
  }
  auto bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value();
  auto client = gcs::Client{};
  (void)client.InsertObject(bucket_name,
                            "aggregate-throughput-benchmark/32KiB.bin",
                            std::string(32 * gcs_bm::kKiB, 'A'));
  return gcs_bm::ParseAggregateThroughputOptions(
      {
          argv0,
          "--bucket-name=" + bucket_name,
          "--object-prefix=aggregate-throughput-benchmark/",
          "--thread-count=1",
          "--reporting-interval=5s",
          "--running-time=15s",
          "--read-size=32KiB",
          "--read-buffer-size=16KiB",
          "--api=JSON",
      },
      kDescription);
}

google::cloud::StatusOr<AggregateThroughputOptions> ParseArgs(int argc,
                                                              char* argv[]) {
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (auto_run) return SelfTest(argv[0]);

  return gcs_bm::ParseAggregateThroughputOptions({argv, argv + argc},
                                                 kDescription);
}

}  // namespace
