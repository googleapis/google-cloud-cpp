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

#include "google/cloud/storage/benchmarks/aggregate_upload_throughput_options.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "google/cloud/testing_util/timer.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

namespace {
using ::google::cloud::storage_experimental::DefaultGrpcClient;
using ::google::cloud::testing_util::FormatSize;
using ::google::cloud::testing_util::Timer;
namespace gcs = ::google::cloud::storage;
namespace gcs_experimental = ::google::cloud::storage_experimental;
namespace gcs_bm = ::google::cloud::storage_benchmarks;
using gcs_bm::AggregateUploadThroughputOptions;
using gcs_bm::ApiName;
using gcs_bm::FormatBandwidthGbPerSecond;

char const kDescription[] = R"""(A benchmark for aggregated upload throughput.

This benchmark repeatedly uploads a dataset to GCS, and reports the time taken
to upload each object, as well as the time taken to upload the dataset.

The benchmark uses multiple threads to upload the dataset, expecting higher
throughput as threads are added. The benchmark runs multiple iterations of the
same workload. After each iteration it prints the upload time for each object,
with arbitrary annotations describing the library configuration (API, buffer
sizes, the iteration number), as well as arbitrary labels provided by the
application, and the overall results for the iteration ("denormalized" to
simplify any external scripts used in analysis).

During each iteration the benchmark keeps a pool of objects to upload, and each
threads pulls objects from this pool as they complete their previous work.

The data for each object is pre-generated and used by all threads, and consist
of a repeating block of N lines with random ASCII characters. The size of this
block is configurable in the command-line. We recommend using multiples of
256KiB for this block size.
)""";

google::cloud::StatusOr<AggregateUploadThroughputOptions> ParseArgs(
    int argc, char* argv[]);

struct TaskConfig {
  gcs::Client client;
};

struct UploadItem {
  std::string object_name;
  std::uint64_t object_size;
};

using Counters = std::map<std::string, std::int64_t>;

struct UploadDetail {
  int iteration;
  std::string peer;
  std::uint64_t bytes_uploaded;
  std::chrono::microseconds elapsed_time;
  google::cloud::Status status;
};

struct TaskResult {
  std::uint64_t bytes_uploaded = 0;
  std::vector<UploadDetail> details;
  Counters counters;
};

class UploadIteration {
 public:
  UploadIteration(int iteration, AggregateUploadThroughputOptions options,
                  std::vector<UploadItem> upload_items)
      : iteration_(iteration),
        options_(std::move(options)),
        remaining_work_(std::move(upload_items)) {}

  TaskResult UploadTask(TaskConfig const& config,
                        std::string const& write_block);

 private:
  std::mutex mu_;
  int const iteration_;
  AggregateUploadThroughputOptions const options_;
  std::vector<UploadItem> remaining_work_;
};

gcs::Client MakeClient(AggregateUploadThroughputOptions const& options) {
  auto client_options =
      google::cloud::Options{}
          // Make the upload buffer size small, the library will flush on almost
          // all `.write()` requests.
          .set<gcs::UploadBufferSizeOption>(256 * gcs_bm::kKiB)
          .set<gcs_experimental::HttpVersionOption>(options.rest_http_version);
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
  if (options->exit_after_parse) return 0;

  auto client = MakeClient(*options);

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
            << "\n# Object Count: " << options->object_count
            << "\n# Minimum Object Size: "
            << FormatSize(options->minimum_object_size)
            << "\n# Maximum Object Size: "
            << FormatSize(options->maximum_object_size)
            << "\n# Resumable Upload Chunk Size: "
            << FormatSize(options->resumable_upload_chunk_size)
            << "\n# Thread Count: " << options->thread_count
            << "\n# Iterations: " << options->iteration_count
            << "\n# API: " << gcs_bm::ToString(options->api)
            << "\n# gRPC Channel Count: " << options->grpc_channel_count
            << "\n# gRPC Plugin Config: " << options->grpc_plugin_config
            << "\n# HTTP Version: " << options->rest_http_version
            << "\n# Client Per Thread: " << std::boolalpha
            << options->client_per_thread << "\n# Build Info: " << notes
            << std::endl;

  auto configs = [](AggregateUploadThroughputOptions const& options,
                    gcs::Client const& default_client) {
    std::vector<TaskConfig> config(options.thread_count,
                                   TaskConfig{default_client});
    if (!options.client_per_thread) return config;
    for (auto& c : config) c.client = MakeClient(options);
    return config;
  }(*options, client);

  std::vector<UploadItem> upload_items(options->object_count);
  std::mt19937_64 generator(std::random_device{}());
  std::generate(upload_items.begin(), upload_items.end(), [&] {
    auto const object_size = std::uniform_int_distribution<std::uint64_t>(
        options->minimum_object_size, options->maximum_object_size)(generator);
    return UploadItem{options->object_prefix +
                          gcs_bm::MakeRandomObjectName(generator).substr(0, 32),
                      object_size};
  });
  auto const write_block = [&] {
    std::string block;
    std::int64_t lineno = 0;
    while (block.size() <
           static_cast<std::size_t>(options->resumable_upload_chunk_size)) {
      // Create data that consists of equally-sized, numbered lines.
      auto constexpr kLineSize = 128;
      auto header = absl::StrFormat("%09d", lineno++);
      block += header;
      block += gcs_bm::MakeRandomData(generator, kLineSize - header.size());
    }
    return block;
  }();

  auto accumulate_bytes_uploaded = [](std::vector<TaskResult> const& r) {
    return std::accumulate(r.begin(), r.end(), std::int64_t{0},
                           [](std::int64_t a, TaskResult const& b) {
                             return a + b.bytes_uploaded;
                           });
  };

  Counters accumulated;
  // Print the header, so it can be easily loaded using the tools available in
  // our analysis tools (typically Python pandas, but could be R). Flush the
  // header because sometimes we interrupt the benchmark and these tools
  // require a header even for empty files.
  std::cout
      << "Iteration,Labels,ObjectCount"
      << ",ResumableUploadChunkSize,ThreadCount,Api"
      << ",GrpcChannelCount,GrpcPluginConfig,RestHttpVersion"
      << ",ClientPerThread,StatusCode,Peer,BytesUploaded,ElapsedMicroseconds"
      << ",IterationBytes,IterationElapsedMicroseconds,IterationCpuMicroseconds"
      << std::endl;

  for (int i = 0; i != options->iteration_count; ++i) {
    auto timer = Timer::PerProcess();
    UploadIteration iteration(i, *options, upload_items);
    auto task = [&iteration, &write_block](TaskConfig const& c) {
      return iteration.UploadTask(c, write_block);
    };
    std::vector<std::future<TaskResult>> tasks(configs.size());
    std::transform(configs.begin(), configs.end(), tasks.begin(),
                   [&task](TaskConfig const& c) {
                     return std::async(std::launch::async, task, std::cref(c));
                   });

    std::vector<TaskResult> iteration_results(configs.size());
    std::transform(std::make_move_iterator(tasks.begin()),
                   std::make_move_iterator(tasks.end()),
                   iteration_results.begin(),
                   [](std::future<TaskResult> f) { return f.get(); });
    auto const usage = timer.Sample();
    auto const uploaded_bytes = accumulate_bytes_uploaded(iteration_results);

    auto clean_csv_field = [](std::string v) {
      std::replace(v.begin(), v.end(), ',', ';');
      return v;
    };
    auto const labels = clean_csv_field(options->labels);
    auto const grpc_plugin_config =
        clean_csv_field(options->grpc_plugin_config);
    auto const* client_per_thread =
        options->client_per_thread ? "true" : "false";
    // Print the results after each iteration. Makes it possible to interrupt
    // the benchmark in the middle and still get some data.
    for (auto const& r : iteration_results) {
      for (auto const& d : r.details) {
        // Join the iteration details with the per-upload details. That makes
        // it easier to analyze the data in external scripts.
        std::cout << d.iteration                                  //
                  << ',' << labels                                //
                  << ',' << options->object_count                 //
                  << ',' << options->resumable_upload_chunk_size  //
                  << ',' << options->thread_count                 //
                  << ',' << ToString(options->api)                //
                  << ',' << options->grpc_channel_count           //
                  << ',' << grpc_plugin_config                    //
                  << ',' << options->rest_http_version            //
                  << ',' << client_per_thread                     //
                  << ',' << d.status.code()                       //
                  << ',' << d.peer                                //
                  << ',' << d.bytes_uploaded                      //
                  << ',' << d.elapsed_time.count()                //
                  << ',' << uploaded_bytes                        //
                  << ',' << usage.elapsed_time.count()            //
                  << ',' << usage.cpu_time.count()                //
                  << "\n";
      }
      // Update the counters.
      for (auto const& kv : r.counters) accumulated[kv.first] += kv.second;
    }
    // After each iteration print a human-readable summary. Flush it because
    // the operator of these benchmarks (coryan@) is an impatient person.
    auto const bandwidth =
        FormatBandwidthGbPerSecond(uploaded_bytes, usage.elapsed_time);
    std::cout << "# " << current_time() << " uploaded=" << uploaded_bytes
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

UploadDetail UploadOneObject(gcs::Client& client,
                             AggregateUploadThroughputOptions const& options,
                             UploadItem const& upload,
                             std::string const& write_block, int iteration) {
  using clock = std::chrono::steady_clock;
  using std::chrono::duration_cast;
  using std::chrono::microseconds;

  auto const buffer_size = static_cast<std::streamsize>(write_block.size());
  // The JSON API returns the object metadata after an insert, while the XML API
  // does not. If the application explicitly requests "filter out all the
  // fields" from the response, then both APIs are equivalent and the library
  // prefers XML in that case. Using gcs::Fields() has no effect.
  auto xml_hack =
      options.api == ApiName::kApiJson ? gcs::Fields() : gcs::Fields("");
  auto const object_start = clock::now();

  auto stream =
      client.WriteObject(options.bucket_name, upload.object_name, xml_hack);
  auto object_bytes = std::uint64_t{0};
  while (object_bytes < upload.object_size) {
    auto n = std::min(static_cast<std::uint64_t>(buffer_size),
                      upload.object_size - object_bytes);
    if (!stream.write(write_block.data(), n)) break;
    object_bytes += n;
  }
  stream.Close();
  // Flush the logs, if any.
  if (!stream.metadata().ok()) google::cloud::LogSink::Instance().Flush();
  auto const object_elapsed =
      duration_cast<microseconds>(clock::now() - object_start);
  auto p = stream.headers().find(":grpc-context-peer");
  if (p == stream.headers().end()) {
    p = stream.headers().find(":curl-peer");
  }
  auto const& peer =
      p == stream.headers().end() ? std::string{"unknown"} : p->second;
  return UploadDetail{iteration, peer, object_bytes, object_elapsed,
                      stream.metadata().status()};
}

TaskResult UploadIteration::UploadTask(TaskConfig const& config,
                                       std::string const& write_block) {
  auto client = config.client;

  TaskResult result;
  while (true) {
    std::unique_lock<std::mutex> lk(mu_);
    if (remaining_work_.empty()) break;
    auto const upload = std::move(remaining_work_.back());
    remaining_work_.pop_back();
    lk.unlock();
    result.details.push_back(
        UploadOneObject(client, options_, upload, write_block, iteration_));
    result.bytes_uploaded += result.details.back().bytes_uploaded;
  }
  return result;
}

using ::google::cloud::internal::GetEnv;

google::cloud::StatusOr<AggregateUploadThroughputOptions> SelfTest(
    char const* argv0) {
  using ::google::cloud::internal::Sample;

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
  return gcs_bm::ParseAggregateUploadThroughputOptions(
      {
          argv0,
          "--bucket-name=" + bucket_name,
          "--object-prefix=aggregate-throughput-benchmark/",
          "--object-count=1",
          "--minimum-object-size=16KiB",
          "--maximum-object-size=32KiB",
          "--thread-count=1",
          "--iteration-count=1",
          "--api=JSON",
          "--grpc-channel-count=1",
          "--grpc-plugin-config=dp",
      },
      kDescription);
}

google::cloud::StatusOr<AggregateUploadThroughputOptions> ParseArgs(
    int argc, char* argv[]) {
  auto const auto_run =
      GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES").value_or("") == "yes";
  if (auto_run) return SelfTest(argv[0]);

  return gcs_bm::ParseAggregateUploadThroughputOptions({argv, argv + argc},
                                                       kDescription);
}

}  // namespace
