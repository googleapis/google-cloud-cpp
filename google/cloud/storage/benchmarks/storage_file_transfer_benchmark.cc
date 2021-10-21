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
#include "google/cloud/storage/testing/remove_stale_buckets.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <fstream>
#include <future>
#include <iomanip>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;

char const kDescription[] = R"""(
A throughput benchmark for the Google Cloud Storage C++ client library.

This program benchmarks the Google Cloud Storage (GCS) C++ client library when
used to upload and download files. The program creates a file of a prescribed
size, and then repeatedly uploads that file to a GCS object, and then downloads
the GCS object to a separate file. The program reports the time taken to perform
the each operation, as well as the effective bandwidth (in Gbps and MiB/s). The
program deletes the target GCS object after each iteration.

To perform this benchmark the program creates a new standard bucket, in a region
configured via the command line. Other test parameters, such as the project id,
the file size, and the buffer sizes are configurable via the command line too.

The bucket name, the local file names, and the object names are all randomly
generated, so multiple instances of the program can run simultaneously. The
output of this program is an annotated CSV file, that can be analyzed by an
external script. The annotation lines start with a '#', analysis scripts should
skip these lines.
)""";

struct Options {
  std::string project_id;
  std::string region;
  std::chrono::seconds duration = std::chrono::seconds(60);
  std::int64_t file_size = 100 * gcs_bm::kMiB;
  std::size_t download_buffer_size = 16 * gcs_bm::kMiB;
  std::size_t upload_buffer_size = 16 * gcs_bm::kMiB;
};

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<Options> options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }

  auto client = gcs::Client(
      google::cloud::Options{}
          .set<gcs::UploadBufferSizeOption>(options->upload_buffer_size)
          .set<gcs::DownloadBufferSizeOption>(options->download_buffer_size)
          .set<gcs::ProjectIdOption>(options->project_id));

  std::cout << "# Cleaning up stale benchmark buckets\n";
  google::cloud::storage::testing::RemoveStaleBuckets(
      client, gcs_bm::RandomBucketPrefix(),
      std::chrono::system_clock::now() - std::chrono::hours(48));

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
            << "\n# File Size: " << options->file_size
            << "\n# File Size (MiB): " << options->file_size / gcs_bm::kMiB
            << "\n# Download buffer size (KiB): "
            << options->download_buffer_size / gcs_bm::kKiB
            << "\n# Upload buffer size (KiB): "
            << options->upload_buffer_size / gcs_bm::kKiB
            << "\n# Build info: " << notes << "\n";

  std::cout << "# Creating file to upload ..." << std::flush;
  auto filename = gcs_bm::MakeRandomFileName(generator);
  {
    auto const filler = gcs_bm::MakeRandomData(generator, 4 * gcs_bm::kMiB);
    std::ofstream os(filename, std::ios::binary);
    std::int64_t current_size = 0;
    while (current_size < options->file_size) {
      auto n = filler.size();
      if (static_cast<std::int64_t>(n) > (options->file_size - current_size)) {
        n = static_cast<std::size_t>(options->file_size - current_size);
      }
      os.write(filler.data(), n);
      current_size += n;
    }
  }
  std::cout << " DONE\n"
            << "# File: " << filename << "\n";

  auto deadline = std::chrono::system_clock::now() + options->duration;
  for (auto now = std::chrono::system_clock::now(); now < deadline;
       now = std::chrono::system_clock::now()) {
    auto object_name = gcs_bm::MakeRandomObjectName(generator);

    auto upload_start = std::chrono::steady_clock::now();
    auto object_metadata =
        client.UploadFile(filename, bucket_name, object_name);
    auto upload_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - upload_start);
    // Convert to Gbps
    auto to_gbps = [](std::int64_t size, std::chrono::nanoseconds elapsed) {
      return 8.0 * static_cast<double>(size) /
             static_cast<double>(elapsed.count());
    };
    // Convert to MiB/s
    auto to_mibs = [](std::int64_t size, std::chrono::milliseconds elapsed) {
      return (static_cast<double>(size) / gcs_bm::kMiB) /
             (static_cast<double>(elapsed.count()) / 1000.0);
    };
    double gbps = to_gbps(options->file_size, upload_elapsed);
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(upload_elapsed);
    auto mi_bs = to_mibs(options->file_size, ms);
    std::cout << "FileUpload," << options->file_size << ','
              << upload_elapsed.count() << ',' << gbps << ',' << ms.count()
              << ',' << mi_bs << ',' << object_metadata.status().code() << "\n";
    if (!object_metadata) {
      std::cout << "# Error in FileUpload: " << object_metadata.status()
                << "\n";
      continue;
    }

    auto destination_filename = gcs_bm::MakeRandomFileName(generator);
    auto download_start = std::chrono::steady_clock::now();
    client.DownloadToFile(object_metadata->bucket(), object_metadata->name(),
                          destination_filename);
    auto download_elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - download_start);
    gbps = to_gbps(options->file_size, download_elapsed);
    ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(download_elapsed);
    mi_bs = to_mibs(options->file_size, ms);
    std::cout << "FileDownload," << options->file_size << ','
              << download_elapsed.count() << ',' << gbps << ',' << ms.count()
              << ',' << mi_bs << ',' << object_metadata.status().code() << "\n";

    (void)client.DeleteObject(object_metadata->bucket(),
                              object_metadata->name(),
                              gcs::Generation(object_metadata->generation()));
    std::remove(destination_filename.c_str());
  }

  std::remove(filename.c_str());

  std::cout << "# Deleting " << bucket_name << "\n";
  auto status = client.DeleteBucket(bucket_name);
  if (!status.ok()) {
    std::cerr << "# Error deleting bucket, status=" << status << "\n";
    return 1;
  }

  return 0;
}

namespace {

using ::google::cloud::testing_util::OptionDescriptor;

google::cloud::StatusOr<Options> ParseArgsDefault(
    std::vector<std::string> const& argv) {
  Options options;

  bool wants_help = false;
  bool wants_description = false;
  std::vector<OptionDescriptor> descriptors{
      {"--help", "print the usage message",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print a description of the benchmark",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--project-id", "the GCP project to create the bucket",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--duration", "how long should the benchmark run (in seconds).",
       [&options](std::string const& val) {
         options.duration = gcs_bm::ParseDuration(val);
       }},
      {"--file-size", "the size of the file to upload",
       [&options](std::string const& val) {
         options.file_size = gcs_bm::ParseSize(val);
       }},
      {"--upload-buffer-size", "configure gcs::Client upload buffer size",
       [&options](std::string const& val) {
         options.upload_buffer_size = gcs_bm::ParseBufferSize(val);
       }},
      {"--download-buffer-size", "configure gcs::Client download buffer size",
       [&options](std::string const& val) {
         options.download_buffer_size = gcs_bm::ParseBufferSize(val);
       }},
      {"--region", "The GCS region used for the benchmark",
       [&options](std::string const& val) { options.region = val; }},
  };
  auto usage = BuildUsage(descriptors, argv[0]);

  auto unparsed = OptionsParse(descriptors, argv);
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
      "--duration=1s",
      "--file-size=1KiB",
      "--upload-buffer-size=1KiB",
      "--download-buffer-size=1KiB",
      "--region=" + GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID").value(),
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
