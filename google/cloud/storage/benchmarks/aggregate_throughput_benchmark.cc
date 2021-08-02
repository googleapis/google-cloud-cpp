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
using ::google::cloud::testing_util::FormatSize;
using ::google::cloud::testing_util::Timer;
namespace gcs = google::cloud::storage;
namespace gcs_experimental = google::cloud::storage_experimental;
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
that is, a collection of GCS objects contained in the same bucket. For this
benchmark, all the objects with a common prefix are part of the same "dataset".
If needed, synthetic datasets can be created using the `create_dataset.cc` in
this directory. Given a dataset and some configuration parameters the program
will:

1) Read the list of available objects in the dataset.
2) Run `iteration-count` iterations where many threads download these objects
   in parallel.
3) Report the effective bandwidth from each iteration.
4) Report additional counters and metrics, such as observed bandwidth per peer.

To run each iteration the benchmark performs the following steps:

a) Split the objects into `thread-count` groups, each group being approximately
   of the same size.
b) Start one thread for each group.
c) Each thread creates a `gcs::Client`, as configured by the
   `AggregateThroughputOptions`.
d) The thread downloads the objects in its group, discarding their data, but
   capturing the download time, size, status, and peer for each download.
e) The thread returns the vector of results at the end of the upload.
)""";

google::cloud::StatusOr<AggregateThroughputOptions> ParseArgs(int argc,
                                                              char* argv[]);

struct TaskConfig {
  gcs::Client client;
  std::seed_seq::result_type seed;
  std::vector<gcs::ObjectMetadata> objects;
};

using Counters = std::map<std::string, std::int64_t>;

struct DownloadDetail {
  int iteration;
  std::string peer;
  std::uint64_t bytes_downloaded;
  std::chrono::microseconds elapsed_time;
  google::cloud::Status status;
};

struct TaskResult {
  std::uint64_t bytes_downloaded = 0;
  std::chrono::microseconds elapsed_time = std::chrono::microseconds(0);
  std::vector<DownloadDetail> details;
  Counters counters;
};

TaskResult DownloadTask(AggregateThroughputOptions const& options,
                        TaskConfig const& config, int iteration);

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

gcs::Client MakeClient(AggregateThroughputOptions const& options) {
  auto client_options =
      google::cloud::Options{}.set<gcs_experimental::HttpVersionOption>(
          options.rest_http_version);
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (options.api == ApiName::kApiGrpc) {
    auto channels = options.grpc_channel_count;
    if (channels == 0) channels = (std::max)(options.thread_count / 4, 4);
    client_options.set<google::cloud::GrpcNumChannelsOption>(channels)
        .set<google::cloud::storage_experimental::GrpcPluginOption>(
            options.grpc_plugin_config);
    return DefaultGrpcClient(std::move(client_options));
  }
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  return gcs::Client(std::move(client_options));
}

}  // namespace

int main(int argc, char* argv[]) {
  auto options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }
  if (options->exit_after_parse) return 1;

  auto client = MakeClient(*options);
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
            << "\n# Labels: " << options->labels
            << "\n# Bucket Name: " << options->bucket_name
            << "\n# Object Prefix: " << options->object_prefix
            << "\n# Thread Count: " << options->thread_count
            << "\n# Iterations: " << options->iteration_count
            << "\n# Repeats Per Iteration: " << options->repeats_per_iteration
            << "\n# Read Size: " << options->read_size
            << "\n# Read Buffer Size: " << options->read_buffer_size
            << "\n# API: " << gcs_bm::ToString(options->api)
            << "\n# gRPC Channel Count: " << options->grpc_channel_count
            << "\n# gRPC Plugin Config: " << options->grpc_plugin_config
            << "\n# Build Info: " << notes
            << "\n# Object Count: " << objects.size()
            << "\n# Dataset size: " << FormatSize(dataset_size) << std::endl;

  auto configs = [](AggregateThroughputOptions const& options,
                    gcs::Client const& default_client,
                    std::vector<gcs::ObjectMetadata> const& objects) {
    std::random_device rd;
    std::vector<std::seed_seq::result_type> seeds(options.thread_count);
    std::seed_seq({rd(), rd(), rd()}).generate(seeds.begin(), seeds.end());

    auto default_config = TaskConfig{default_client, 0, {}};
    std::vector<TaskConfig> config(seeds.size(), default_config);
    for (std::size_t i = 0; i != config.size(); ++i) {
      if (options.client_per_thread) config[i].client = MakeClient(options);
      config[i].seed = seeds[i];
    }
    std::size_t pos = 0;
    for (int j = 0; j != options.repeats_per_iteration; ++j) {
      for (auto const& o : objects) {
        auto const target = pos++ % config.size();
        config[target].objects.push_back(o);
      }
    }
    return config;
  }(*options, client, objects);

  auto accumulate_bytes_downloaded = [](std::vector<TaskResult> const& r) {
    return std::accumulate(r.begin(), r.end(), std::int64_t{0},
                           [](std::int64_t a, TaskResult const& b) {
                             return a + b.bytes_downloaded;
                           });
  };

  Counters accumulated;
  // Print the header, so it can be easily loaded using the tools available in
  // our analysis tools (typically Python pandas, but could be R). Flush the
  // header because sometimes we interrupt the benchmark and these tools
  // require a header even for empty files.
  std::cout << "Labels,Iteration,ObjectCount,DatasetSize,ThreadCount"
               ",RepeatsPerIteration,ReadSize,ReadBufferSize,Api"
               ",GrpcChannelCount,GrpcPluginConfig,StatusCode,Peer"
               ",BytesDownloaded,ElapsedMicroseconds,IterationBytes"
               ",IterationElapsedMicroseconds,IterationCpuMicroseconds"
            << std::endl;
  for (int i = 0; i != options->iteration_count; ++i) {
    auto timer = Timer::PerProcess();
    std::vector<std::future<TaskResult>> tasks(configs.size());
    std::transform(configs.begin(), configs.end(), tasks.begin(),
                   [&](TaskConfig const& c) {
                     return std::async(std::launch::async, DownloadTask,
                                       *options, std::cref(c), i);
                   });
    std::vector<TaskResult> iteration_results(configs.size());
    std::transform(std::make_move_iterator(tasks.begin()),
                   std::make_move_iterator(tasks.end()),
                   iteration_results.begin(),
                   [](std::future<TaskResult> f) { return f.get(); });
    auto const usage = timer.Sample();
    auto const downloaded_bytes =
        accumulate_bytes_downloaded(iteration_results);

    auto clean_csv_field = [](std::string v) {
      std::replace(v.begin(), v.end(), ',', ';');
      return v;
    };
    auto const labels = clean_csv_field(options->labels);
    auto const grpc_plugin_config =
        clean_csv_field(options->grpc_plugin_config);
    // Print the results after each iteration. Makes it possible to interrupt
    // the benchmark in the middle and still get some data.
    for (auto const& r : iteration_results) {
      for (auto const& d : r.details) {
        // Join the iteration details with the per-download details. That makes
        // it easier to analyze the data in external scripts.
        std::cout << labels                                 //
                  << ',' << d.iteration                     //
                  << ',' << objects.size()                  //
                  << ',' << dataset_size                    //
                  << ',' << options->thread_count           //
                  << ',' << options->repeats_per_iteration  //
                  << ',' << options->read_size              //
                  << ',' << options->read_buffer_size       //
                  << ',' << ToString(options->api)          //
                  << ',' << options->grpc_channel_count     //
                  << ',' << grpc_plugin_config              //
                  << ',' << d.status.code()                 //
                  << ',' << d.peer                          //
                  << ',' << d.bytes_downloaded              //
                  << ',' << d.elapsed_time.count()          //
                  << ',' << downloaded_bytes                //
                  << ',' << usage.elapsed_time.count()      //
                  << ',' << usage.cpu_time.count()          //
                  << "\n";
      }
      // Update the counters.
      for (auto const& kv : r.counters) accumulated[kv.first] += kv.second;
    }
    // After each iteration print a human-readable summary. Flush it because
    // the operator of these benchmarks (coryan@) is an impatient person.
    auto const bandwidth =
        FormatBandwidthGbPerSecond(downloaded_bytes, usage.elapsed_time);
    std::cout << "# " << current_time() << " downloaded=" << downloaded_bytes
              << " cpu_time=" << absl::FromChrono(usage.cpu_time)
              << " elapsed_time=" << absl::FromChrono(usage.elapsed_time)
              << " Gbit/s=" << bandwidth << std::endl;
  }

  for (auto& kv : accumulated) {
    std::cout << "# counter " << kv.first << ": " << kv.second << "\n";
  }
  return 0;
}

namespace {

TaskResult DownloadTask(AggregateThroughputOptions const& options,
                        TaskConfig const& config, int iteration) {
  auto client = config.client;

  TaskResult result;
  std::vector<char> buffer(options.read_buffer_size);
  auto const buffer_size = static_cast<std::streamsize>(buffer.size());
  auto generator = std::mt19937_64(config.seed);
  // Using IfGenerationNotMatch(0) triggers JSON, as this feature is not
  // supported by XML.  Using IfGenerationNotMatch() -- without a value -- has
  // no effect.
  auto xml_hack = options.api == ApiName::kApiJson
                      ? gcs::IfGenerationNotMatch(0)
                      : gcs::IfGenerationNotMatch();

  using clock = std::chrono::steady_clock;
  using std::chrono::duration_cast;
  using std::chrono::microseconds;
  auto const start = clock::now();
  auto const unknown_peer = std::string{"unknown"};
  for (auto const& object : config.objects) {
    auto const object_start = clock::now();
    auto object_bytes = std::uint64_t{0};
    auto const object_size = static_cast<std::int64_t>(object.size());
    auto range = gcs::ReadRange();
    if (options.read_size != 0 && options.read_size < object_size) {
      auto read_start = std::uniform_int_distribution<std::int64_t>(
          0, object_size - options.read_size);
      range = gcs::ReadRange(read_start(generator), options.read_size);
    }
    auto stream = client.ReadObject(object.bucket(), object.name(),
                                    gcs::Generation(object.generation()), range,
                                    xml_hack);
    while (stream.read(buffer.data(), buffer_size)) {
      object_bytes += stream.gcount();
    }
    stream.Close();
    result.bytes_downloaded += object_bytes;
    auto const object_elapsed =
        duration_cast<microseconds>(clock::now() - object_start);
    auto p = stream.headers().find(":grpc-context-peer");
    if (p == stream.headers().end()) {
      p = stream.headers().find(":curl-peer");
    }
    auto const& peer = p == stream.headers().end() ? unknown_peer : p->second;
    result.details.push_back(DownloadDetail{iteration, peer, object_bytes,
                                            object_elapsed, stream.status()});
  }
  result.elapsed_time = duration_cast<microseconds>(clock::now() - start);
  return result;
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
          "--iteration-count=1",
          "--read-size=32KiB",
          "--read-buffer-size=16KiB",
          "--api=JSON",
          "--grpc-channel-count=1",
          "--grpc-plugin-config=dp",
      },
      kDescription);
}

google::cloud::StatusOr<AggregateThroughputOptions> ParseArgs(int argc,
                                                              char* argv[]) {
  auto const auto_run =
      GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES").value_or("") == "yes";
  if (auto_run) return SelfTest(argv[0]);

  return gcs_bm::ParseAggregateThroughputOptions({argv, argv + argc},
                                                 kDescription);
}

}  // namespace
