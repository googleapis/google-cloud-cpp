// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/benchmarks/throughput_experiment.h"
#include "google/cloud/storage/benchmarks/throughput_options.h"
#include "google/cloud/storage/benchmarks/throughput_result.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <future>
#include <set>
#include <sstream>

namespace {
namespace gcs = ::google::cloud::storage;
namespace gcs_ex = ::google::cloud::storage_experimental;
namespace gcs_bm = ::google::cloud::storage_benchmarks;
using gcs_bm::ExperimentLibrary;
using gcs_bm::ExperimentTransport;
using gcs_bm::ThroughputOptions;
using gcs_bm::ThroughputResult;

char const kDescription[] = R"""(
A throughput vs. CPU benchmark for the Google Cloud Storage C++ client library.

This program measures the throughput and CPU utilization when uploading
and downloading objects using the Google Cloud Storage (GCS) C++ client library.
The program repeats the "experiment" of uploading, then downloading, and then
removing an object many times, using a randomly selected size in each iteration.
An external script presents these results as a series of plots.

The program first creates a GCS bucket that will contain all the objects used
by that run of the program. The name of this bucket is selected at random, so
multiple copies of the program can run simultaneously. The bucket is deleted at
the end of the run of this program. The bucket uses the `STANDARD` storage
class, in a region set via the command line. Choosing regions close to where the
program is running can be used to estimate the latency without any wide-area
network effects. Choosing regions far from where the program is running can be
used to evaluate the performance of the library when the wide-area network is
taken into account.

After creating this bucket the program creates a number of threads, configurable
via the command line, to obtain more samples in parallel. Configure this value
with a small enough number of threads such that you do not saturate the CPU.

Each thread creates C++ objects to perform the "upload experiments". Each one
of these objects represents the "api" used to perform the upload, that is XML,
JSON and/or gRPC (though technically gRPC is just another protocol for the JSON
API). Likewise, the thread creates a number of "download experiments", also
based on the APIs configured via the command-line.

Then the thread repeats the following steps (see below for the conditions to
stop the loop):

- Select a random size, between two values configured in the command line of the
  object to upload.
- The application buffer sizes for `read()` and `write()` calls are also
  selected at random. These sizes are quantized, and the quantum can be
  configured in the command-line.
- Select a random uploader, that is, which API will be used to upload the
  object.
- Select a random downloader, that is, which API will be used to download the
  object.
- Select, at random, if the client library will perform CRC32C and/or MD5 hashes
  on the uploaded and downloaded data.
- Upload an object of the selected size, choosing the name of the object at
  random.
- Once the object is fully uploaded, the program captures the object size, the
  write buffer size, the elapsed time (in microseconds), the CPU time
  (in microseconds) used during the upload, and the status code for the upload.
- Then the program downloads the same object (3 times), and captures the object
  size, the read buffer size, the elapsed time (in microseconds), the CPU time
  (in microseconds) used during the download, and the status code for the
  download.
- The program then deletes this object and starts another iteration.

The loop stops when any of the following conditions are met:

- The test has obtained more than a prescribed "maximum number of samples"
- The test has obtained at least a prescribed "minimum number of samples" *and*
  the test has been running for more than a prescribed "duration".

Once the threads finish running their loops the program prints the captured
performance data. The bucket is deleted after the program terminates.

A helper script in this directory can generate pretty graphs from the output of
this program.
)""";

using TestResults = std::vector<ThroughputResult>;

TestResults RunThread(ThroughputOptions const& ThroughputOptions,
                      std::string const& bucket_name, int thread_id);
void PrintResults(TestResults const& results);

google::cloud::StatusOr<ThroughputOptions> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<ThroughputOptions> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }

  auto client_options =
      google::cloud::Options{}.set<gcs::ProjectIdOption>(options->project_id);
  auto client = gcs::Client(client_options);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto bucket_name = gcs_bm::MakeRandomBucketName(generator);
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  struct Formatter {
    void operator()(std::string* out, ExperimentLibrary v) const {
      out->append(gcs_bm::ToString(v));
    }
    void operator()(std::string* out, ExperimentTransport v) const {
      out->append(gcs_bm::ToString(v));
    }
    void operator()(std::string* out, bool b) const {
      out->append(b ? "true" : "false");
    }
  };

  std::cout << "# Running test on bucket: " << bucket_name << "\n# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Region: " << options->region
            << "\n# Duration: " << options->duration.count() << "s"
            << "\n# Thread Count: " << options->thread_count
            << "\n# Object Size Range: [" << options->minimum_object_size << ","
            << options->maximum_object_size << "]\n# Write Size Range: ["
            << options->minimum_write_size << "," << options->maximum_write_size
            << "]\n# Write Quantum: " << options->write_quantum
            << "\n# Min Read Size Range: [" << options->minimum_read_size << ","
            << options->maximum_read_size
            << "]\n# Read Quantum: " << options->read_quantum
            << "\n# Object Size Range (MiB): ["
            << options->minimum_object_size / gcs_bm::kMiB << ","
            << options->maximum_object_size / gcs_bm::kMiB
            << "]\n# Write Size Range (KiB): ["
            << options->minimum_write_size / gcs_bm::kKiB << ","
            << options->maximum_write_size / gcs_bm::kKiB
            << "]\n# Write Quantum (KiB): "
            << options->write_quantum / gcs_bm::kKiB
            << "\n# Min Read Size Range (KiB): ["
            << options->minimum_read_size / gcs_bm::kKiB << ","
            << options->maximum_read_size / gcs_bm::kKiB
            << "]\n# Read Quantum (KiB): "
            << options->read_quantum / gcs_bm::kKiB
            << "\n# Minimum Sample Count: " << options->minimum_sample_count
            << "\n# Maximum Sample Count: " << options->maximum_sample_count
            << "\n# Enabled Libs: "
            << absl::StrJoin(options->libs, ",", Formatter{})
            << "\n# Enabled Transports: "
            << absl::StrJoin(options->transports, ",", Formatter{})
            << "\n# Enabled CRC32C: "
            << absl::StrJoin(options->enabled_crc32c, ",", Formatter{})
            << "\n# Enabled MD5: "
            << absl::StrJoin(options->enabled_md5, ",", Formatter{})
            << "\n# REST Endpoint: " << options->rest_endpoint
            << "\n# Grpc Endpoint: " << options->grpc_endpoint
            << "\n# Direct Path Endpoint: " << options->direct_path_endpoint
            << "\n# Build info: " << notes << "\n";
  // Make the output generated so far immediately visible, helps with debugging.
  std::cout << std::flush;

  auto meta =
      client.CreateBucket(bucket_name,
                          gcs::BucketMetadata()
                              .set_storage_class(gcs::storage_class::Standard())
                              .set_location(options->region),
                          gcs::PredefinedAcl::ProjectPrivate(),
                          gcs::PredefinedDefaultObjectAcl::ProjectPrivate(),
                          gcs::Projection("full"));
  if (!meta) {
    std::cerr << "Error creating bucket: " << meta.status() << "\n";
    return 1;
  }

  gcs_bm::PrintThroughputResultHeader(std::cout);
  std::vector<std::future<TestResults>> tasks;
  for (int i = 0; i != options->thread_count; ++i) {
    tasks.emplace_back(
        std::async(std::launch::async, RunThread, *options, bucket_name, i));
  }
  for (auto& f : tasks) {
    PrintResults(f.get());
  }

  gcs_bm::DeleteAllObjects(client, bucket_name, options->thread_count);
  auto status = client.DeleteBucket(bucket_name);
  if (!status.ok()) {
    std::cerr << "# Error deleting bucket, status=" << status << "\n";
    return 1;
  }
  std::cout << "# DONE\n" << std::flush;

  return 0;
}

namespace {

void PrintResults(TestResults const& results) {
  for (auto const& r : results) {
    gcs_bm::PrintAsCsv(std::cout, r);
  }
  std::cout << std::flush;
}

gcs_bm::ClientProvider MakeProvider(ThroughputOptions const& options) {
  return [=](ExperimentTransport t) {
    auto opts =
        google::cloud::Options{}.set<gcs::ProjectIdOption>(options.project_id);
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    using ::google::cloud::storage_experimental::DefaultGrpcClient;
    if (t == ExperimentTransport::kDirectPath) {
      return DefaultGrpcClient(opts.set<gcs_ex::GrpcPluginOption>("media")
                                   .set<google::cloud::EndpointOption>(
                                       options.direct_path_endpoint));
    }
    if (t == ExperimentTransport::kGrpc) {
      return DefaultGrpcClient(
          opts.set<gcs_ex::GrpcPluginOption>("media")
              .set<google::cloud::EndpointOption>(options.grpc_endpoint));
    }
#else
    (void)t;  // disable unused parameter warning
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    return gcs::Client(
        opts.set<gcs::RestEndpointOption>(options.rest_endpoint));
  };
}

TestResults RunThread(ThroughputOptions const& options,
                      std::string const& bucket_name, int thread_id) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cout << "# Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return {};
  }
  auto const upload_buffer_size = client_options->upload_buffer_size();
  auto const download_buffer_size = client_options->download_buffer_size();

  auto provider = MakeProvider(options);

  auto uploaders = gcs_bm::CreateUploadExperiments(options, provider);
  if (uploaders.empty()) {
    // This is possible if only gRPC is requested but the benchmark was compiled
    // without gRPC support.
    std::cout << "# None of the APIs configured are available\n";
    return {};
  }
  auto downloaders =
      gcs_bm::CreateDownloadExperiments(options, provider, thread_id);
  if (downloaders.empty()) {
    // This is possible if only gRPC is requested but the benchmark was compiled
    // without gRPC support.
    std::cout << "# None of the APIs configured are available\n";
    return {};
  }

  std::uniform_int_distribution<std::size_t> uploader_generator(
      0, uploaders.size() - 1);
  std::uniform_int_distribution<std::size_t> downloader_generator(
      0, downloaders.size() - 1);

  std::uniform_int_distribution<std::int64_t> size_generator(
      options.minimum_object_size, options.maximum_object_size);
  std::uniform_int_distribution<std::size_t> write_size_generator(
      options.minimum_write_size / options.write_quantum,
      options.maximum_write_size / options.write_quantum);
  std::uniform_int_distribution<std::size_t> read_size_generator(
      options.minimum_read_size / options.read_quantum,
      options.maximum_read_size / options.read_quantum);

  std::uniform_int_distribution<std::size_t> crc32c_generator(
      0, options.enabled_crc32c.size() - 1);
  std::uniform_int_distribution<std::size_t> md5_generator(
      0, options.enabled_crc32c.size() - 1);
  std::bernoulli_distribution use_insert;

  auto deadline = std::chrono::steady_clock::now() + options.duration;

  TestResults results;

  std::int32_t iteration_count = 0;
  for (auto start = std::chrono::steady_clock::now();
       iteration_count < options.maximum_sample_count &&
       (iteration_count < options.minimum_sample_count || start < deadline);
       start = std::chrono::steady_clock::now(), ++iteration_count) {
    auto object_name = gcs_bm::MakeRandomObjectName(generator);
    auto object_size = size_generator(generator);
    auto write_size = options.write_quantum * write_size_generator(generator);
    auto read_size = options.read_quantum * read_size_generator(generator);
    bool const enable_crc = options.enabled_crc32c[crc32c_generator(generator)];
    bool const enable_md5 = options.enabled_md5[md5_generator(generator)];

    auto& uploader = uploaders[uploader_generator(generator)];
    auto upload_result =
        uploader->Run(bucket_name, object_name,
                      gcs_bm::ThroughputExperimentConfig{
                          gcs_bm::kOpWrite, object_size, write_size,
                          upload_buffer_size, enable_crc, enable_md5});
    auto status = upload_result.status;
    results.emplace_back(std::move(upload_result));

    if (!status.ok()) {
      if (options.thread_count == 1) {
        std::cout << "# status=" << status << "\n";
      }
      continue;
    }

    auto& downloader = downloaders[downloader_generator(generator)];
    for (auto op : {gcs_bm::kOpRead0, gcs_bm::kOpRead1, gcs_bm::kOpRead2}) {
      results.emplace_back(downloader->Run(
          bucket_name, object_name,
          gcs_bm::ThroughputExperimentConfig{op, object_size, read_size,
                                             download_buffer_size, enable_crc,
                                             enable_md5}));
    }
    if (options.thread_count == 1) {
      // Immediately print the results, this makes it easier to debug problems.
      PrintResults(results);
      results.clear();
    }
    auto client = provider(ExperimentTransport::kJson);
    (void)client.DeleteObject(bucket_name, object_name);
  }
  return results;
}

google::cloud::StatusOr<ThroughputOptions> SelfTest(char const* argv0) {
  using ::google::cloud::internal::GetEnv;
  using ::google::cloud::internal::Sample;

  for (auto const& var :
       {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID"}) {
    auto const value = GetEnv(var).value_or("");
    if (!value.empty()) continue;
    std::ostringstream os;
    os << "The environment variable " << var << " is not set or empty";
    return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                 std::move(os).str());
  }
  return gcs_bm::ParseThroughputOptions(
      {
          argv0,
          "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value(),
          "--region=" +
              GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID").value(),
          "--thread-count=1",
          "--minimum-object-size=16KiB",
          "--maximum-object-size=32KiB",
          "--minimum-write-size=16KiB",
          "--maximum-write-size=128KiB",
          "--write-quantum=16KiB",
          "--minimum-read-size=16KiB",
          "--maximum-read-size=128KiB",
          "--read-quantum=16KiB",
          "--duration=1s",
          "--minimum-sample-count=4",
          "--maximum-sample-count=10",
          "--enabled-transports=Json,Xml",
          "--enabled-crc32c=enabled",
          "--enabled-md5=disabled",
      },
      kDescription);
}

google::cloud::StatusOr<ThroughputOptions> ParseArgs(int argc, char* argv[]) {
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (auto_run) return SelfTest(argv[0]);

  return gcs_bm::ParseThroughputOptions({argv, argv + argc}, kDescription);
}

}  // namespace
