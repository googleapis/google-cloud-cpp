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
#include "google/cloud/storage/benchmarks/throughput_options.h"
#include "google/cloud/storage/benchmarks/throughput_result.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/str_join.h"
#include <future>
#include <set>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;
using gcs_bm::ApiName;
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
Each thread creates a separate copy of the `storage::Client` object, and repeats
this loop (see below for the conditions to stop the loop):

- Select a random size, between two values configured in the command line of the
  object to upload.
- The application buffer sizes for `read()` and `write()` calls are also
  selected at random. These sizes are quantized, and the quantum can be
  configured in the command-line.
- Select, at random, the protocol / API used to perform the test, could be XML,
  JSON, or gRPC.
- Select, at random, if the client library will perform CRC32C and/or MD5 hashes
  on the uploaded and downloaded data.
- Upload an object of the selected size, choosing the name of the object at
  random.
- Once the object is fully uploaded, the program captures the object size, the
  write buffer size, the elapsed time (in microseconds), the CPU time
  (in microseconds) used during the upload, and the status code for the upload.
- Then the program downloads the same object, and captures the object size, the
  read buffer size, the elapsed time (in microseconds), the CPU time (in
  microseconds) used during the download, and the status code for the download.
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
                      std::string const& bucket_name);
void PrintResults(TestResults const& results);

google::cloud::StatusOr<ThroughputOptions> ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::StatusOr<ThroughputOptions> options = ParseArgs(argc, argv);
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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto bucket_name =
      gcs_bm::MakeRandomBucketName(generator, options->bucket_prefix);
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

  struct Formatter {
    void operator()(std::string* out, ApiName api) const {
      out->append(gcs_bm::ToString(api));
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
            << "\n# Min Object Size: " << options->minimum_object_size
            << "\n# Max Object Size: " << options->maximum_object_size
            << "\n# Min Write Size: " << options->minimum_write_size
            << "\n# Max Write Size: " << options->maximum_write_size
            << "\n# Write Quantum: " << options->write_quantum
            << "\n# Min Read Size: " << options->minimum_read_size
            << "\n# Max Read Size: " << options->maximum_read_size
            << "\n# Read Quantum: " << options->read_quantum
            << "\n# Min Object Size (MiB): "
            << options->minimum_object_size / gcs_bm::kMiB
            << "\n# Max Object Size (MiB): "
            << options->maximum_object_size / gcs_bm::kMiB
            << "\n# Min Write Size (KiB): "
            << options->minimum_write_size / gcs_bm::kKiB
            << "\n# Max Write Size (KiB): "
            << options->maximum_write_size / gcs_bm::kKiB
            << "\n# Write Quantum (KiB): "
            << options->write_quantum / gcs_bm::kKiB
            << "\n# Min Read Size (KiB): "
            << options->minimum_read_size / gcs_bm::kKiB
            << "\n# Max Read Size (KiB): "
            << options->maximum_read_size / gcs_bm::kKiB
            << "\n# Read Quantum (KiB): "
            << options->read_quantum / gcs_bm::kKiB
            << "\n# Minimum Sample Count: " << options->minimum_sample_count
            << "\n# Maximum Sample Count: " << options->maximum_sample_count
            << "\n# Enabled APIs: "
            << absl::StrJoin(options->enabled_apis, ",", Formatter{})
            << "\n# Enabled CRC32C: "
            << absl::StrJoin(options->enabled_crc32c, ",", Formatter{})
            << "\n# Enabled MD5: "
            << absl::StrJoin(options->enabled_md5, ",", Formatter{})
            << "\n# Build info: " << notes << "\n";
  // Make the output generated so far immediately visible, helps with debugging.
  std::cout << std::flush;

  auto meta =
      client.CreateBucket(bucket_name,
                          gcs::BucketMetadata()
                              .set_storage_class(gcs::storage_class::Standard())
                              .set_location(options->region),
                          gcs::PredefinedAcl("private"),
                          gcs::PredefinedDefaultObjectAcl("projectPrivate"),
                          gcs::Projection("full"));
  if (!meta) {
    std::cerr << "Error creating bucket: " << meta.status() << "\n";
    return 1;
  }

  gcs_bm::PrintThroughputResultHeader(std::cout);
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

void PrintResults(TestResults const& results) {
  for (auto const& r : results) {
    gcs_bm::PrintAsCsv(std::cout, r);
  }
  std::cout << std::flush;
}

TestResults RunThread(ThroughputOptions const& options,
                      std::string const& bucket_name) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto contents = gcs_bm::MakeRandomData(generator, options.maximum_write_size);
  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!client_options) {
    std::cout << "# Could not create ClientOptions, status="
              << client_options.status() << "\n";
    return {};
  }
  std::uint64_t upload_buffer_size = client_options->upload_buffer_size();
  std::uint64_t download_buffer_size = client_options->download_buffer_size();

  gcs::Client rest_client(*client_options);
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  gcs::Client grpc_client =
      google::cloud::storage_experimental::DefaultGrpcClient(
          *std::move(client_options));
#else
  gcs::Client grpc_client(rest_client);
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRP

  std::uniform_int_distribution<std::int64_t> size_generator(
      options.minimum_object_size, options.maximum_object_size);
  std::uniform_int_distribution<std::int64_t> write_size_generator(
      options.minimum_write_size / options.write_quantum,
      options.maximum_write_size / options.write_quantum);
  std::uniform_int_distribution<std::int64_t> read_size_generator(
      options.minimum_read_size / options.read_quantum,
      options.maximum_read_size / options.read_quantum);

  std::uniform_int_distribution<std::size_t> crc32c_generator(
      0, options.enabled_crc32c.size() - 1);
  std::uniform_int_distribution<std::size_t> md5_generator(
      0, options.enabled_crc32c.size() - 1);
  std::bernoulli_distribution use_insert;

  std::uniform_int_distribution<std::size_t> api_generator(
      0, options.enabled_apis.size() - 1);

  auto deadline = std::chrono::steady_clock::now() + options.duration;

  gcs_bm::SimpleTimer timer;
  // This obviously depends on the size of the objects, but a good estimate for
  // the upload + download bandwidth is 250MiB/s.
  constexpr auto kExpectedBandwidth = 250 * gcs_bm::kMiB;
  auto const median_size =
      (options.minimum_object_size + options.minimum_object_size) / 2;
  auto const objects_per_second = median_size / kExpectedBandwidth;
  TestResults results;
  results.reserve(options.duration.count() * objects_per_second);

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
    auto const api = options.enabled_apis[api_generator(generator)];
    auto xml_write_selector = gcs::Fields();
    auto json_read_selector = gcs::IfGenerationNotMatch();

    auto* client = &rest_client;
    switch (api) {
      case gcs_bm::ApiName::kApiXml:
        // For writes (uploads) the default is JSON, but if the application is
        // not interested in any fields on the metadata the library can switch
        // to XML and does.
        xml_write_selector = gcs::Fields("");
        break;

      case gcs_bm::ApiName::kApiJson:
        // For reads (downloads) the default is XML, unless the application is
        // using features that only JSON supports, like `IfGenerationNotMatch`.
        json_read_selector = gcs::IfGenerationNotMatch(0);
        break;

      case gcs_bm::ApiName::kApiGrpc:
        client = &grpc_client;
        break;
    }

    auto perform_upload = [&options, &contents, client, bucket_name,
                           object_name, object_size, write_size, enable_crc,
                           enable_md5, xml_write_selector](bool prefer_insert) {
      // When the object is relatively small using `ObjectInsert` might be more
      // efficient. Randomly select about 1/2 of the small writes to use
      // ObjectInsert()
      if (object_size < options.maximum_write_size && prefer_insert) {
        std::string data = contents.substr(0, object_size);
        auto object_metadata = client->InsertObject(
            bucket_name, object_name, std::move(data),
            gcs::DisableCrc32cChecksum(!enable_crc),
            gcs::DisableMD5Hash(!enable_md5), xml_write_selector);
        return std::make_pair(gcs_bm::kOpInsert, std::move(object_metadata));
      }
      auto writer = client->WriteObject(
          bucket_name, object_name, gcs::DisableCrc32cChecksum(!enable_crc),
          gcs::DisableMD5Hash(!enable_md5), xml_write_selector);
      for (std::int64_t offset = 0; offset < object_size;
           offset += write_size) {
        auto len = write_size;
        if (offset + len > object_size) {
          len = object_size - offset;
        }
        writer.write(contents.data(), len);
      }
      writer.Close();
      return std::make_pair(gcs_bm::kOpWrite, writer.metadata());
    };

    timer.Start();
    auto upload_result = perform_upload(use_insert(generator));
    timer.Stop();

    auto object_metadata = std::move(upload_result.second);
    results.emplace_back(ThroughputResult{
        upload_result.first, object_size, write_size, upload_buffer_size,
        enable_crc, enable_md5, api, timer.elapsed_time(), timer.cpu_time(),
        object_metadata.status().code()});

    if (!object_metadata) {
      if (options.thread_count == 1) {
        std::cerr << "# status=" << object_metadata.status()
                  << ", api=" << gcs_bm::ToString(api) << "\n";
      }
      continue;
    }

    for (auto op : {gcs_bm::kOpRead0, gcs_bm::kOpRead1, gcs_bm::kOpRead2}) {
      timer.Start();
      auto reader = client->ReadObject(
          bucket_name, object_name, gcs::DisableCrc32cChecksum(!enable_crc),
          gcs::DisableMD5Hash(!enable_md5), json_read_selector);
      std::vector<char> buffer(read_size);
      for (size_t num_read = 0; reader.read(buffer.data(), buffer.size());
           num_read += reader.gcount()) {
      }
      timer.Stop();
      results.emplace_back(
          ThroughputResult{op, object_size, read_size, download_buffer_size,
                           enable_crc, enable_md5, api, timer.elapsed_time(),
                           timer.cpu_time(), reader.status().code()});

      if (options.thread_count == 1 && !reader.status().ok()) {
        std::cerr << "# status=" << reader.status() << "\n"
                  << "# metadata=" << *object_metadata << "\n"
                  << "# json_read_selector=" << json_read_selector.has_value()
                  << "\n";
      }
    }
    if (options.thread_count == 1) {
      // Immediately print the results, this makes it easier to debug problems.
      PrintResults(results);
      results.clear();
    }

    auto status = client->DeleteObject(bucket_name, object_name);
  }
  return results;
}

google::cloud::StatusOr<ThroughputOptions> SelfTest(char const* argv0) {
  using google::cloud::internal::GetEnv;
  using google::cloud::internal::Sample;

  for (auto const& var :
       {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID"}) {
    auto const value = GetEnv(var).value_or("");
    if (!value.empty()) continue;
    std::ostringstream os;
    os << "The environment variable " << var << " is not set or empty";
    return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                 std::move(os).str());
  }
  auto const* const thread_count_arg =
      gcs_bm::SimpleTimer::SupportPerThreadUsage() ? "--thread-count=2"
                                                   : "--thread-count=1";
  return gcs_bm::ParseThroughputOptions(
      {
          argv0,
          "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value(),
          "--region=" +
              GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID").value(),
          "--bucket-prefix=cloud-cpp-testing-ci-",
          thread_count_arg,
          "--minimum-object-size=16KiB",
          "--maximum-object-size=32KiB",
          "--minimum-write-size=16KiB",
          "--maximum-write-size=128KiB",
          "--write-quantum=16KiB",
          "--minimum-read-size=16KiB",
          "--maximum-read-size=128KiB",
          "--read-quantum=16KiB",
          "--duration=1s",
          "--minimum-sample-count=1",
          "--maximum-sample-count=2",
          "--enabled-apis=JSON,GRPC,XML",
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
