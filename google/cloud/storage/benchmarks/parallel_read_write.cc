// Copyright 2018 Google LLC
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

#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/format_time_point.h"
#include <future>
#include <iomanip>
#include <sstream>

/**
 * @file
 *
 * A latency benchmark for the Google Cloud Storage C++ client library.
 *
 * This program measures the latency to upload and download small (~1 MiB)
 * objects to Google Cloud Storage using the C++ client library. The program
 * repeats the "experiment" of uploading or downloading the file many times,
 * and reports all the results of this experiment. An external script performs
 * statistical analysis on the results to estimate likely values for p95 and p99
 * of the latency.
 *
 * The program first creates a Bucket that will contain all the Objects used in
 * the test.  The Bucket is deleted at the end of the test. The name of the
 * Bucket is selected at random, that way multiple instances of this test can
 * run simultaneously. The Bucket uses the `REGIONAL` storage class, in a region
 * set via the command-line.
 *
 * After creating this Bucket the program creates a prescribed number of
 * objects, selecting random names for all these objects. All the objects have
 * the same contents, but the contents are generated at random.
 *
 * Once the object creation phase is completed, the program starts N threads,
 * each thread executes a simple loop:
 * - Pick one of the objects at random, with equal probability for each Object.
 * - Pick, with equal probably, at action (`read` or `write`) at random.
 * - If the action was `write` then write a new version of the object.
 * - If the action was `read` then read the given object.
 * - Capture the time taken to read and/or write the object.
 *
 * The loop runs for a prescribed number of seconds, at the end of the loop the
 * program prints the captured performance data.
 *
 * Then the program remotes all the objects in the bucket, and reports the time
 * taken to delete each one.
 *
 * A helper script in this directory can generate pretty graphs from the report.
 */

namespace {
namespace gcs = google::cloud::storage;

constexpr std::chrono::seconds kDefaultDuration(60);
constexpr long kDefaultObjectCount = 1000;
constexpr long kBlobSize = 1024 * 1024;

struct Options {
  std::string project_id;
  std::string region;
  std::chrono::seconds duration;
  long object_count;
  int thread_count;
  bool enable_connection_pool;
  bool enable_xml_api;

  Options()
      : duration(kDefaultDuration),
        object_count(kDefaultObjectCount),
        enable_connection_pool(true),
        enable_xml_api(true) {
    thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) {
      thread_count = 1;
    }
  }

  void ParseArgs(int& argc, char* argv[]);
  std::string ConsumeArg(int& argc, char* argv[], char const* arg_name);
};

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen);
std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size);
std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen);

}  // namespace

int main(int argc, char* argv[]) try {
  Options options;
  options.ParseArgs(argc, argv);

  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cerr << "Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return 1;
  }
  if (!options.enable_connection_pool) {
    client_options->set_connection_pool_size(0);
  }
  gcs::Client client(*std::move(client_options));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name = MakeRandomBucketName(generator);
  auto meta =
      client
          .CreateBucket(bucket_name,
                        gcs::BucketMetadata()
                            .set_storage_class(gcs::storage_class::Regional())
                            .set_location(options.region),
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
            << gcs::internal::FormatRfc3339(std::chrono::system_clock::now())
            << "\n# Region: " << options.region
            << "\n# Object Count: " << options.object_count
            << "\n# Thread Count: " << options.thread_count
            << "\n# Enable connection pool: " << options.enable_connection_pool
            << "\n# Enable XML API: " << options.enable_xml_api
            << "\n# Build info: " << notes << "\n";

  std::cout << "# Deleting " << bucket_name << "\n";
  auto status = client.DeleteBucket(bucket_name);
  if (!status.ok()) {
    google::cloud::internal::ThrowStatus(status);
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}


namespace {
std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen) {
  // The total length of this bucket name must be <= 63 characters,
  static std::string const prefix = "gcs-cpp-latency-";
  static std::size_t const kMaxBucketNameLength = 63;
  std::size_t const max_random_characters =
      kMaxBucketNameLength - prefix.size();
  return prefix + google::cloud::internal::Sample(
      gen, static_cast<int>(max_random_characters),
      "abcdefghijklmnopqrstuvwxyz012456789");
}

std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size) {
  std::string result;
  result.reserve(desired_size);

  // Create lines of 128 characters to start with, we can fill the remaining
  // characters at the end.
  constexpr int kLineSize = 128;
  auto gen_random_line = [&gen](std::size_t count) {
    return google::cloud::internal::Sample(gen, static_cast<int>(count - 1),
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "012456789"
                                           " - _ : /") +
        "\n";
  };
  while (result.size() + kLineSize < desired_size) {
    result += gen_random_line(kLineSize);
  }
  if (result.size() < desired_size) {
    result += gen_random_line(desired_size - result.size());
  }

  return result;
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen) {
  return google::cloud::internal::Sample(gen, 128,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789");
}
}