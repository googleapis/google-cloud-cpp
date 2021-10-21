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
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/terminate_handler.h"
#include <cstdio>
#include <future>
#include <iomanip>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;

char const kDescription[] = R"""(
A parallel uploads benchmark for the Google Cloud Storage C++ client library.

This program measures the throughput of uploading objects via
ParallelUploadFile() API from Google Cloud Storage C++ client library. The
program repeats the "experiment" of uploading a file many times, using a
randomly selected size and parallelism in each iteration. An
external script may perform statistical analysis on the results to estimate a
function for the proper number of shards.

The program first creates a GCS bucket that will contain all the objects used
by that run of the program. The name of this bucket is selected at random, so
multiple copies of the program can run simultaneously. The bucket is deleted at
the end of the run of this program. The bucket uses the `STANDARD` storage
class, in a region set via the command line. Choosing regions close to where the
program is running can be used to estimate the throughput without any wide-area
network effects. Choosing regions far from where the program is running can be
used to evaluate the performance of the library when the WAN is taken into
account.

After creating this bucket the program creates a number of threads, configurable
via the command line, to obtain more samples in parallel. Configure this value
with a small enough number of threads such that you do not saturate the CPU or
memory. Each thread creates a separate copy of the `storage::Client` object
repeats this loop:

- Select a random size, between two values configured in the command line of the
  object to upload.
- Select a random number of shards, between two values configured in the command
  line, this is the level of parallelism of each upload.
- Upload an object of the selected size, choosing the name of the object at
  random.
- Once the object is fully uploaded, the program captures the object size,
  the elapsed time (in microseconds), used during the upload, and the status
  code for the upload.
- The program then deletes this object and starts another iteration.

The loop stops when any of the following conditions are met:

- The test has obtained more than a prescribed "maximum number of samples"
- The test has obtained at least a prescribed "minimum number of samples" *and*
  the test has been running for more than a prescribed "duration".

Once the threads finish running their loops the program prints the captured
performance data. The bucket is deleted after the program terminates.
)""";

using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::StatusOr;

struct Options {
  std::string project_id;
  std::string region;
  std::string object_prefix = "parallel-upload-bm-";
  std::string directory = "/tmp/";
  std::chrono::seconds duration =
      std::chrono::seconds(std::chrono::minutes(15));
  int thread_count = 1;
  std::int64_t minimum_object_size = 128 * gcs_bm::kMiB;
  std::int64_t maximum_object_size = 8 * gcs_bm::kGiB;
  std::size_t minimum_num_shards = 1;
  std::size_t maximum_num_shards = 128;
  long minimum_sample_count = 0;  // NOLINT(google-runtime-int)
  // NOLINTNEXTLINE(google-runtime-int)
  long maximum_sample_count = std::numeric_limits<long>::max();
};

StatusOr<std::string> CreateTempFile(
    std::string const& directory,
    google::cloud::internal::DefaultPRNG& generator, std::uintmax_t size_left) {
  std::size_t const single_buf_size = 4 * 1024 * 1024;
  auto const file_name = directory + (directory.empty() ? "" : "/") +
                         gcs_bm::MakeRandomFileName(generator);

  std::string random_data = gcs_bm::MakeRandomData(generator, single_buf_size);

  std::ofstream file(file_name, std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return Status(
        StatusCode::kInternal,
        "Failed to create a temporary file (file_name=" + file_name + ")");
  }
  while (size_left > 0) {
    auto const to_write = [size_left, &random_data] {
      if (size_left > random_data.size()) return random_data.size();
      return static_cast<std::size_t>(size_left);
    }();
    size_left -= to_write;
    file.write(random_data.c_str(), to_write);
    if (!file.good()) {
      std::remove(file_name.c_str());
      return Status(StatusCode::kInternal,
                    "Failed to write to file " + file_name);
    }
  }
  return file_name;
}

Status PerformUpload(gcs::Client& client, std::string const& file_name,
                     std::string const& bucket_name, std::string const& prefix,
                     std::size_t num_shards) {
  if (num_shards == 1) {
    auto res = client.UploadFile(file_name, bucket_name, prefix + ".dest");
    if (!res) {
      return res.status();
    }
    return Status();
  }
  auto object_metadata = gcs::ParallelUploadFile(
      client, file_name, bucket_name, prefix + ".dest", prefix, false,
      gcs::MinStreamSize(0), gcs::MaxStreams(num_shards));
  if (!object_metadata) {
    return object_metadata.status();
  }

  return Status();
}

StatusOr<std::chrono::milliseconds> TimeSingleUpload(
    gcs::Client& client, std::string const& global_prefix,
    std::string const& bucket_name, std::size_t num_shards,
    std::string const& file_name) {
  using std::chrono::milliseconds;

  std::string const& prefix = gcs::CreateRandomPrefixName(global_prefix);

  auto start = std::chrono::steady_clock::now();
  auto status =
      PerformUpload(client, file_name, bucket_name, prefix, num_shards);
  auto elapsed = std::chrono::steady_clock::now() - start;
  if (status.ok()) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
  }
  return status;
}

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<Options> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }

  auto client = gcs::Client(google::cloud::Options{}
                                .set<gcs::ProjectIdOption>(options->project_id)
                                .set<gcs::ConnectionPoolSizeOption>(0));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name = gcs_bm::MakeRandomBucketName(generator);
  auto meta =
      client
          .CreateBucket(bucket_name,
                        gcs::BucketMetadata()
                            .set_storage_class(gcs::storage_class::Standard())
                            .set_location(options->region),
                        gcs::PredefinedAcl::ProjectPrivate(),
                        gcs::PredefinedDefaultObjectAcl::ProjectPrivate(),
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
            << "\n# Min Object Size (MiB): "
            << options->minimum_object_size / gcs_bm::kMiB
            << "\n# Max Object Size (MiB): "
            << options->maximum_object_size / gcs_bm::kMiB
            << "\n# Min Number of Shards: " << options->minimum_num_shards
            << "\n# Max Number of Shards: " << options->maximum_num_shards
            << "\n# Build info: " << notes
            << "\nFileSize,ShardCount,UploadTimeMs\n";

  std::vector<std::thread> threads;
  std::atomic<int> iteration_count(0);
  std::mutex cout_mutex;
  for (int i = 0; i != options->thread_count; ++i) {
    threads.emplace_back([&iteration_count, &cout_mutex, options, bucket_name] {
      google::cloud::internal::DefaultPRNG generator =
          google::cloud::internal::MakeDefaultPRNG();

      auto client = gcs::Client();

      std::uniform_int_distribution<std::uint64_t> size_generator(
          options->minimum_object_size, options->maximum_object_size);
      std::uniform_int_distribution<std::size_t> num_shards_generator(
          options->minimum_num_shards, options->maximum_num_shards);

      auto deadline = std::chrono::steady_clock::now() + options->duration;

      for (auto start = std::chrono::steady_clock::now();
           iteration_count < options->maximum_sample_count &&
           (iteration_count < options->minimum_sample_count ||
            start < deadline);
           start = std::chrono::steady_clock::now(), ++iteration_count) {
        auto const file_size = size_generator(generator);
        auto const num_shards = num_shards_generator(generator);
        auto file_name =
            CreateTempFile(options->directory, generator, file_size);
        if (!file_name) {
          std::lock_guard<std::mutex> lk(cout_mutex);
          std::cout << "# Could not prepare file to upload, size=" << file_size
                    << ", status=" << file_name.status() << "\n";
          return;
        }
        auto time = TimeSingleUpload(client, options->object_prefix,
                                     bucket_name, num_shards, *file_name);
        std::remove(file_name->c_str());
        if (!time) {
          std::lock_guard<std::mutex> lk(cout_mutex);
          std::cout << "# Could not create upload sample file, status="
                    << time.status() << "\n";
          return;
        }
        std::lock_guard<std::mutex> lk(cout_mutex);
        std::cout << file_size << ',' << num_shards << ',' << time->count()
                  << '\n';
      }
    });
  };
  for (auto& thread : threads) {
    thread.join();
  }

  auto status = DeleteByPrefix(client, bucket_name, "", gcs::Versions());
  if (!status.ok()) {
    std::cerr << "# Error deleting bucket leftovers status=" << status << "\n";
    return 1;
  }
  status = client.DeleteBucket(bucket_name);
  if (!status.ok()) {
    std::cerr << "# Error deleting bucket, status=" << status << "\n";
    return 1;
  }
  return 0;
}

namespace {

using ::google::cloud::testing_util::OptionDescriptor;

google::cloud::StatusOr<Options> ParseArgsDefault(
    std::vector<std::string> argv) {
  Options options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--object-prefix", "use the given prefix for created objects",
       [&options](std::string const& val) { options.object_prefix = val; }},
      {"--directory", "use the given directory for files to be uploaded",
       [&options](std::string const& val) { options.directory = val; }},
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
      {"--minimum-num-shards",
       "configure the minimum number of shards in the test",
       [&options](std::string const& val) {
         options.minimum_num_shards = std::stoi(val);
       }},
      {"--maximum-num-shards",
       "configure the maximum number of shards in the test",
       [&options](std::string const& val) {
         options.maximum_num_shards = std::stoi(val);
       }},
      {"--duration", "continue the test for at least this amount of time",
       [&options](std::string const& val) {
         options.duration = gcs_bm::ParseDuration(val);
       }},
      {"--minimum-sample-count",
       "continue the test until at least this number of samples are "
       "obtained",
       [&options](std::string const& val) {
         options.minimum_sample_count = std::stol(val);
       }},
      {"--maximum-sample-count",
       "stop the test when this number of samples are obtained",
       [&options](std::string const& val) {
         options.maximum_sample_count = std::stol(val);
       }},
  };
  auto usage = BuildUsage(desc, argv[0]);

  auto unparsed = OptionsParse(desc, argv);
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
  if (options.minimum_num_shards > options.maximum_num_shards) {
    std::ostringstream os;
    os << "Invalid range for number of shards [" << options.minimum_num_shards
       << ',' << options.maximum_num_shards << "]";
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
    // Shard count range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-num-shards=8",
        "--maximum-num-shards=4",
    });
    if (options) return self_test_error;
  }
  {
    // Sample count range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-sample-count=8",
        "--maximum-sample-count=4",
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
  return ParseArgsDefault({
      "self-test",
      "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value(),
      "--object-prefix=parallel-upload/",
      "--directory=.",
      "--region=" + GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID").value(),
      "--thread-count=2",
      "--minimum-object-size=16KiB",
      "--maximum-object-size=32KiB",
      "--minimum-num-shards=2",
      "--maximum-num-shards=4",
      "--duration=1s",
      "--minimum-sample-count=2",
      "--maximum-sample-count=4",
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
