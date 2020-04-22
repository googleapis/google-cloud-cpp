// Copyright 2019 Google LLC
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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <future>
#include <iomanip>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;

char const kDescription[] = R"""(
A throughput vs. CPU benchmark for the Google Cloud Storage C++ client library.

This program measures the throughput and CPU utilization when uploading
and downloading relatively large (~250 MiB) objects Google Cloud Storage C++
client library. The program repeats the "experiment" of uploading, then
downloading, and then removing a file many times, using a randomly selected
size in each iteration. An external script performs statistical analysis on
the results to estimate likely values for the throughput and the CPU cost per
byte on both upload and download operations.

The program first creates a GCS bucket that will contain all the objects used
by that run of the program. The name of this bucket is selected at random, so
multiple copies of the program can run simultaneously. The bucket is deleted at
the end of the run of this program. The bucket uses the `STANDARD` storage
class, in a region set via the command line. Choosing regions close to where the
program is running can be used to estimate the latency without any wide-area
network effects. Choosing regions far from where the program is running can be
used to evaluate the performance of the library when the WAN is taken into
account.

After creating this bucket the program creates a number of threads, configurable
via the command line, to obtain more samples in parallel. Configure this value
with a small enough number of threads such that you do not saturate the CPU.
Each thread creates a separate copy of the `storage::Client` object, and repeats
this loop until a prescribed *time* has elapsed:

- Select a random size, between two values configured in the command line of the
  object to upload.
- Select a random chunk size, between two values configured in the command line,
  the data is uploaded in chunks of this size.
- Upload an object of the selected size, choosing the name of the object at
  random.
- Once the object is fully uploaded, the program captures the object size, the
  chunk size, the elapsed time (in microseconds), the CPU time (in microseconds)
  used during the upload, and the status code for the upload.
- Then the program downloads the same object, and captures the object size, the
  chunk size, the elapsed time (in microseconds), the CPU time (in microseconds)
  used during the download, and the status code for the download.
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

struct Options {
  std::string project_id;
  std::string region;
  std::chrono::seconds duration =
      std::chrono::seconds(std::chrono::minutes(15));
  int thread_count = 1;
  std::int64_t minimum_object_size = 32 * gcs_bm::kMiB;
  std::int64_t maximum_object_size = 256 * gcs_bm::kMiB;
  std::int64_t minimum_chunk_size = 128 * gcs_bm::kKiB;
  std::int64_t maximum_chunk_size = 4096 * gcs_bm::kKiB;
  long minimum_sample_count = 0;
  long maximum_sample_count = std::numeric_limits<long>::max();
};

enum OpType { OP_UPLOAD, OP_DOWNLOAD };
struct IterationResult {
  OpType op;
  std::uint64_t object_size;
  std::uint64_t chunk_size;
  std::uint64_t buffer_size;
  bool crc_enabled;
  bool md5_enabled;
  std::chrono::microseconds elapsed_time;
  std::chrono::microseconds cpu_time;
  google::cloud::StatusCode status;
  std::vector<gcs_bm::ProgressReporter::TimePoint> progress;
};
using TestResults = std::vector<IterationResult>;

TestResults RunThread(Options const& options, std::string const& bucket_name);
void PrintResults(TestResults const& results);

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<Options> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }

  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cerr << "Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return 1;
  }
  if (!options->project_id.empty()) {
    client_options->set_project_id(options->project_id);
  }
  gcs::Client client(*std::move(client_options));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name =
      gcs_bm::MakeRandomBucketName(generator, "bm-throughput-vs-cpu-");
  auto meta =
      client
          .CreateBucket(bucket_name,
                        gcs::BucketMetadata()
                            .set_storage_class(gcs::storage_class::Standard())
                            .set_location(options->region),
                        gcs::PredefinedAcl("private"),
                        gcs::PredefinedDefaultObjectAcl("projectPrivate"),
                        gcs::Projection("full"))
          .value();
  std::cout << "# Running test on bucket: " << meta.name() << "\n";
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  std::cout << "# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Region: " << options->region
            << "\n# Duration: " << options->duration.count() << "s"
            << "\n# Thread Count: " << options->thread_count
            << "\n# Min Object Size: " << options->minimum_object_size
            << "\n# Max Object Size: " << options->maximum_object_size
            << "\n# Min Chunk Size: " << options->minimum_chunk_size
            << "\n# Max Chunk Size: " << options->maximum_chunk_size
            << "\n# Min Object Size (MiB): "
            << options->minimum_object_size / gcs_bm::kMiB
            << "\n# Max Object Size (MiB): "
            << options->maximum_object_size / gcs_bm::kMiB
            << "\n# Min Chunk Size (KiB): "
            << options->minimum_chunk_size / gcs_bm::kKiB
            << "\n# Max Chunk Size (KiB): "
            << options->maximum_chunk_size / gcs_bm::kKiB << std::boolalpha
            << "\n# Build info: " << notes << "\n";
  // Make this immediately visible in the console, helps with debugging.
  std::cout << std::flush;

  std::vector<std::future<TestResults>> tasks;
  for (int i = 0; i != options->thread_count; ++i) {
    tasks.emplace_back(
        std::async(std::launch::async, RunThread, *options, bucket_name));
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
char const* ToString(OpType type) {
  switch (type) {
    case OP_DOWNLOAD:
      return "DOWNLOAD";
    case OP_UPLOAD:
      return "UPLOAD";
  }
  return nullptr;  // silence g++ error.
}

std::ostream& operator<<(std::ostream& os,
                         gcs_bm::ProgressReporter::TimePoint const& point) {
  return os << point.bytes << '|' << point.elapsed.count();
}

std::ostream& operator<<(
    std::ostream& os,
    std::vector<gcs_bm::ProgressReporter::TimePoint> const& points) {
  char const* sep = "";
  for (auto const& point : points) {
    os << sep << point;
    sep = ";";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, IterationResult const& rhs) {
  return os << ToString(rhs.op) << ',' << rhs.object_size << ','
            << rhs.chunk_size << ',' << rhs.buffer_size << ','
            << rhs.crc_enabled << ',' << rhs.md5_enabled << ','
            << rhs.elapsed_time.count() << ',' << rhs.cpu_time.count() << ','
            << rhs.status << ',' << rhs.progress << ','
            << google::cloud::storage::version_string();
}

void PrintResults(TestResults const& results) {
  for (auto& r : results) {
    std::cout << r << '\n';
  }
  std::cout << std::flush;
}

TestResults RunThread(Options const& options, std::string const& bucket_name) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();
  auto contents =
      gcs_bm::MakeRandomData(generator, options.maximum_object_size);
  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cout << "# Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return {};
  }
  std::uint64_t upload_buffer_size = client_options->upload_buffer_size();
  std::uint64_t download_buffer_size = client_options->download_buffer_size();
  gcs::Client client(*std::move(client_options));

  std::uniform_int_distribution<std::uint64_t> size_generator(
      options.minimum_object_size, options.maximum_object_size);
  std::uniform_int_distribution<std::uint64_t> chunk_generator(
      options.minimum_chunk_size, options.maximum_chunk_size);

  std::bernoulli_distribution crc_generator;
  std::bernoulli_distribution md5_generator;

  auto deadline = std::chrono::steady_clock::now() + options.duration;

  gcs_bm::SimpleTimer timer;
  // This obviously depends on the size of the objects, but a good estimate for
  // the upload + download bandwidth is 250MiB/s.
  constexpr long expected_bandwidth = 250 * gcs_bm::kMiB;
  auto const median_size =
      (options.minimum_object_size + options.minimum_object_size) / 2;
  auto const objects_per_second = median_size / expected_bandwidth;
  TestResults results;
  results.reserve(options.duration.count() * objects_per_second);

  long iteration_count = 0;
  for (auto start = std::chrono::steady_clock::now();
       iteration_count < options.maximum_sample_count &&
       (iteration_count < options.minimum_sample_count || start < deadline);
       start = std::chrono::steady_clock::now(), ++iteration_count) {
    auto object_name = gcs_bm::MakeRandomObjectName(generator);
    auto object_size = size_generator(generator);
    auto chunk_size = chunk_generator(generator);
    bool enable_crc = crc_generator(generator);
    bool enable_md5 = md5_generator(generator);

    gcs_bm::ProgressReporter progress;
    timer.Start();
    progress.Start();
    auto writer = client.WriteObject(bucket_name, object_name,
                                     gcs::DisableCrc32cChecksum(!enable_crc),
                                     gcs::DisableMD5Hash(!enable_md5));
    for (std::size_t offset = 0; offset < object_size; offset += chunk_size) {
      auto len = chunk_size;
      if (offset + len > object_size) {
        len = object_size - offset;
      }
      writer.write(contents.data() + offset, len);
      progress.Advance(offset);
    }
    writer.Close();
    progress.Advance(object_size);
    timer.Stop();

    auto object_metadata = writer.metadata();
    results.emplace_back(IterationResult{
        OP_UPLOAD, object_size, chunk_size, download_buffer_size, enable_crc,
        enable_md5, timer.elapsed_time(), timer.cpu_time(),
        object_metadata.status().code(), progress.GetAccumulatedProgress()});

    if (!object_metadata) {
      continue;
    }

    timer.Start();
    progress.Start();
    auto reader =
        client.ReadObject(object_metadata->bucket(), object_metadata->name(),
                          gcs::Generation(object_metadata->generation()),
                          gcs::DisableCrc32cChecksum(!enable_crc),
                          gcs::DisableMD5Hash(!enable_md5));
    progress.Advance(0);
    std::vector<char> buffer(chunk_size);
    for (size_t num_read = 0; reader.read(buffer.data(), buffer.size());
         progress.Advance(num_read += reader.gcount())) {
    }
    timer.Stop();
    results.emplace_back(IterationResult{
        OP_DOWNLOAD, object_size, chunk_size, upload_buffer_size, enable_crc,
        enable_md5, timer.elapsed_time(), timer.cpu_time(),
        reader.status().code(), progress.GetAccumulatedProgress()});

    auto status =
        client.DeleteObject(object_metadata->bucket(), object_metadata->name(),
                            gcs::Generation(object_metadata->generation()));

    if (options.thread_count == 1) {
      // Immediately print the results, this makes it easier to debug problems.
      PrintResults(results);
      results.clear();
    }
  }
  return results;
}

google::cloud::StatusOr<Options> ParseArgsDefault(
    std::vector<std::string> argv) {
  Options options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<gcs_bm::OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--region", "use the given region for the benchmark",
       [&options](std::string const& val) { options.region = val; }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--minimum-object-size", "configure the minimum object size in the test",
       [&options](std::string const& val) {
         options.minimum_object_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-object-size", "configure the maximum object size in the test",
       [&options](std::string const& val) {
         options.maximum_object_size = gcs_bm::ParseSize(val);
       }},
      {"--minimum-chunk-size", "configure the minimum chunk size in the test",
       [&options](std::string const& val) {
         options.minimum_chunk_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-chunk-size", "configure the maximum chunk size in the test",
       [&options](std::string const& val) {
         options.maximum_chunk_size = gcs_bm::ParseSize(val);
       }},
      {"--duration", "continue the test for at least this amount of time",
       [&options](std::string const& val) {
         options.duration = gcs_bm::ParseDuration(val);
       }},
      {"--minimum-sample-count",
       "continue the test until at least this number of samples are obtained",
       [&options](std::string const& val) {
         options.minimum_sample_count = std::stol(val);
       }},
      {"--maximum-sample-count",
       "stop the test when this number of samples are obtained",
       [&options](std::string const& val) {
         options.maximum_sample_count = std::stol(val);
       }},
  };
  auto usage = gcs_bm::BuildUsage(desc, argv[0]);

  auto unparsed = gcs_bm::OptionsParse(desc, argv);
  if (wants_help) {
    std::cout << usage << "\n";
  }

  if (wants_description) {
    std::cout << kDescription << "\n";
  }

  if (unparsed.size() > 2) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (unparsed.size() == 2) {
    options.region = unparsed[1];
  }
  if (options.region.empty()) {
    std::ostringstream os;
    os << "Missing value for --region option" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  if (options.minimum_object_size > options.maximum_object_size) {
    std::ostringstream os;
    os << "Invalid range for object size [" << options.minimum_object_size
       << ',' << options.maximum_object_size << "]";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (options.minimum_chunk_size > options.maximum_chunk_size) {
    std::ostringstream os;
    os << "Invalid range for chunk size [" << options.minimum_chunk_size << ','
       << options.maximum_chunk_size << "]";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (options.minimum_sample_count > options.maximum_sample_count) {
    std::ostringstream os;
    os << "Invalid range for sample range [" << options.minimum_sample_count
       << ',' << options.maximum_sample_count << "]";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  if (!gcs_bm::SimpleTimer::SupportPerThreadUsage() &&
      options.thread_count > 1) {
    std::ostringstream os;
    os << "Your platform does not support per-thread usage metrics"
          " (see getrusage(2)). Running more than one thread is not supported.";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }

  return options;
}

google::cloud::StatusOr<Options> SelfTest() {
  using google::cloud::internal::GetEnv;
  using google::cloud::internal::Sample;

  google::cloud::Status const self_test_error(
      google::cloud::StatusCode::kUnknown, "self-test failure");

  {
    auto options = ParseArgsDefault(
        {"self-test", "--help", "--description", "fake-region"});
    if (!options) return options;
  }
  {
    // Missing the region should be an error
    auto options = ParseArgsDefault({"self-test"});
    if (options) return self_test_error;
  }
  {
    // Too many positional arguments should be an error
    auto options = ParseArgsDefault({"self-test", "unused-1", "unused-2"});
    if (options) return self_test_error;
  }
  {
    // Object size range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-object-size=8",
        "--maximum-object-size=4",
    });
    if (options) return self_test_error;
  }
  {
    // Chunk size range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-chunk-size=8",
        "--maximum-chunk-size=4",
    });
    if (options) return self_test_error;
  }
  {
    // Sample count range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-sample-size=8",
        "--maximum-sample-size=4",
    });
    if (options) return self_test_error;
  }

  for (auto const& var :
       {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID"}) {
    auto const value = GetEnv(var).value_or("");
    if (!value.empty()) continue;
    std::ostringstream os;
    os << "The environment variable " << var << " is not set or empty";
    return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                 std::move(os).str());
  }
  auto const thread_count_arg = gcs_bm::SimpleTimer::SupportPerThreadUsage()
                                    ? "--thread-count=2"
                                    : "--thread-count=1";
  return ParseArgsDefault({
      "self-test",
      "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value(),
      "--region=" + GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID").value(),
      thread_count_arg,
      "--minimum-object-size=16KiB",
      "--maximum-object-size=32KiB",
      "--minimum-chunk-size=1KiB",
      "--maximum-chunk-size=2KiB",
      "--duration=1s",
      "--minimum-sample-count=1",
      "--maximum-sample-count=2",
  });
}

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]) {
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (auto_run) return SelfTest();

  return ParseArgsDefault({argv, argv + argc});
}

}  // namespace
