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
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/str_split.h"
#include <future>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;
using gcs_bm::ApiName;

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

struct Options {
  std::string project_id;
  std::string region;
  std::string bucket_prefix = "cloud-cpp-testing-bm-";
  std::chrono::seconds duration =
      std::chrono::seconds(std::chrono::minutes(15));
  int thread_count = 1;
  std::int64_t minimum_object_size = 32 * gcs_bm::kMiB;
  std::int64_t maximum_object_size = 256 * gcs_bm::kMiB;
  std::int64_t minimum_write_size = 16 * gcs_bm::kMiB;
  std::int64_t maximum_write_size = 64 * gcs_bm::kMiB;
  std::int64_t write_quantum = 256 * gcs_bm::kKiB;
  std::int64_t minimum_read_size = 4 * gcs_bm::kMiB;
  std::int64_t maximum_read_size = 8 * gcs_bm::kMiB;
  std::int64_t read_quantum = 1 * gcs_bm::kMiB;
  std::int32_t minimum_sample_count = 0;
  std::int32_t maximum_sample_count = std::numeric_limits<std::int32_t>::max();
  std::vector<ApiName> enabled_apis = {
      gcs_bm::ApiName::kApiJson,
      gcs_bm::ApiName::kApiXml,
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
      gcs_bm::ApiName::kApiGrpc,
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  };
};

enum OpType { kOpWrite, kOpInsert, kOpRead0, kOpRead1, kOpRead2 };
struct IterationResult {
  OpType op;
  std::int64_t object_size;
  std::int64_t app_buffer_size;
  std::uint64_t lib_buffer_size;
  bool crc_enabled;
  bool md5_enabled;
  ApiName api;
  std::chrono::microseconds elapsed_time;
  std::chrono::microseconds cpu_time;
  google::cloud::StatusCode status;
};
using TestResults = std::vector<IterationResult>;

TestResults RunThread(Options const& options, std::string const& bucket_name);
void PrintHeader();
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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto bucket_name =
      gcs_bm::MakeRandomBucketName(generator, options->bucket_prefix);
  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });

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
            << "\n# Enabled APIs: " <<
      [&options] {
        std::string s;
        char const* sep = "";
        for (auto a : options->enabled_apis) {
          s += sep;
          s += gcs_bm::ToString(a);
          sep = ",";
        }
        return s;
      }()
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

  PrintHeader();
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
    case kOpRead0:
      return "READ[0]";
    case kOpRead1:
      return "READ[1]";
    case kOpRead2:
      return "READ[2]";
    case kOpWrite:
      return "WRITE";
    case kOpInsert:
      return "INSERT";
  }
  return nullptr;  // silence g++ error.
}

std::ostream& operator<<(std::ostream& os, IterationResult const& rhs) {
  return os << ToString(rhs.op) << ',' << rhs.object_size << ','
            << rhs.app_buffer_size << ',' << rhs.lib_buffer_size << ','
            << rhs.crc_enabled << ',' << rhs.md5_enabled << ','
            << gcs_bm::ToString(rhs.api) << ',' << rhs.elapsed_time.count()
            << ',' << rhs.cpu_time.count() << ',' << rhs.status;
}

void PrintHeader() {
  std::cout << "Op,ObjectSize,AppBufferSize,LibBufferSize"
            << ",Crc32cEnabled,MD5Enabled,ApiName"
            << ",ElapsedTimeUs,CpuTimeUs,Status\n";
}

void PrintResults(TestResults const& results) {
  for (auto& r : results) {
    std::cout << r << '\n';
  }
  std::cout << std::flush;
}

TestResults RunThread(Options const& options, std::string const& bucket_name) {
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

  std::bernoulli_distribution crc_generator;
  std::bernoulli_distribution md5_generator;
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
    bool enable_crc = crc_generator(generator);
    bool enable_md5 = md5_generator(generator);
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
        return std::make_pair(kOpInsert, std::move(object_metadata));
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
      return std::make_pair(kOpWrite, writer.metadata());
    };

    timer.Start();
    auto upload_result = perform_upload(use_insert(generator));
    timer.Stop();

    auto object_metadata = std::move(upload_result.second);
    results.emplace_back(IterationResult{
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

    for (auto op : {kOpRead0, kOpRead1, kOpRead2}) {
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
          IterationResult{op, object_size, read_size, download_buffer_size,
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
      {"--bucket-prefix", "use the given prefix to create a bucket name",
       [&options](std::string const& val) { options.bucket_prefix = val; }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--minimum-object-size", "configure the minimum object size",
       [&options](std::string const& val) {
         options.minimum_object_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-object-size", "configure the maximum object size",
       [&options](std::string const& val) {
         options.maximum_object_size = gcs_bm::ParseSize(val);
       }},
      {"--minimum-write-size",
       "configure the minimum buffer size for write() calls",
       [&options](std::string const& val) {
         options.minimum_write_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-write-size",
       "configure the maximum buffer size for write() calls",
       [&options](std::string const& val) {
         options.maximum_write_size = gcs_bm::ParseSize(val);
       }},
      {"--write-quantum", "quantize the buffer sizes for write() calls",
       [&options](std::string const& val) {
         options.write_quantum = gcs_bm::ParseSize(val);
       }},
      {"--minimum-read-size",
       "configure the minimum buffer size for read() calls",
       [&options](std::string const& val) {
         options.minimum_read_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-read-size",
       "configure the maximum buffer size for read() calls",
       [&options](std::string const& val) {
         options.maximum_read_size = gcs_bm::ParseSize(val);
       }},
      {"--read-quantum", "quantize the buffer sizes for read() calls",
       [&options](std::string const& val) {
         options.read_quantum = gcs_bm::ParseSize(val);
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
      {"--enabled-apis", "enable a subset of the APIs for the test",
       [&options](std::string const& val) {
         std::map<std::string, gcs_bm::ApiName> const names{
             {"JSON", gcs_bm::ApiName::kApiJson},
             {"XML", gcs_bm::ApiName::kApiXml},
             {"GRPC", gcs_bm::ApiName::kApiGrpc},
         };
         std::vector<ApiName> apis;
         for (auto& token : absl::StrSplit(val, ',')) {
           auto const l = names.find(std::string(token));
           if (l == names.end()) continue;  // Ignore errors for now
           apis.push_back(l->second);
         }
         options.enabled_apis = std::move(apis);
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

  auto make_status = [](std::ostringstream& os) {
    auto const code = google::cloud::StatusCode::kInvalidArgument;
    return google::cloud::Status{code, std::move(os).str()};
  };

  if (unparsed.size() > 2) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    return make_status(os);
  }
  if (unparsed.size() == 2) {
    options.region = unparsed[1];
  }
  if (options.region.empty()) {
    std::ostringstream os;
    os << "Missing value for --region option\n" << usage << "\n";
    return make_status(os);
  }

  if (options.minimum_object_size > options.maximum_object_size) {
    std::ostringstream os;
    os << "Invalid range for object size [" << options.minimum_object_size
       << ',' << options.maximum_object_size << "]";
    return make_status(os);
  }

  if (options.minimum_write_size > options.maximum_write_size) {
    std::ostringstream os;
    os << "Invalid range for write size [" << options.minimum_write_size << ','
       << options.maximum_write_size << "]";
    return make_status(os);
  }
  if (options.write_quantum <= 0 ||
      options.write_quantum > options.minimum_write_size) {
    std::ostringstream os;
    os << "Invalid value for --write-quantum (" << options.write_quantum
       << "), it should be in the [1," << options.minimum_write_size
       << "] range";
    return make_status(os);
  }

  if (options.minimum_read_size > options.maximum_read_size) {
    std::ostringstream os;
    os << "Invalid range for read size [" << options.minimum_read_size << ','
       << options.maximum_read_size << "]";
    return make_status(os);
  }
  if (options.read_quantum <= 0 ||
      options.read_quantum > options.minimum_read_size) {
    std::ostringstream os;
    os << "Invalid value for --read-quantum (" << options.read_quantum
       << "), it should be in the [1," << options.minimum_read_size
       << "] range";
    return make_status(os);
  }

  if (options.minimum_sample_count > options.maximum_sample_count) {
    std::ostringstream os;
    os << "Invalid range for sample range [" << options.minimum_sample_count
       << ',' << options.maximum_sample_count << "]";
    return make_status(os);
  }

  if (!gcs_bm::SimpleTimer::SupportPerThreadUsage() &&
      options.thread_count > 1) {
    std::cerr <<
        R"""(
# WARNING
# Your platform does not support per-thread usage metrics and you have enabled
# multiple threads, so the CPU usage results will not be usable. See
# getrusage(2) for more information.
# END WARNING
#
)""";
  }

  if (options.enabled_apis.empty()) {
    std::ostringstream os;
    os << "No APIs configured for benchmark.";
    return make_status(os);
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
    // Write buffer size range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-write-size=8",
        "--maximum-write-size=4",
    });
    if (options) return self_test_error;
  }
  {
    // Write size quantum is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-write-size=4",
        "--maximum-write-size=8",
        "--write-quantum=5",
    });
    if (options) return self_test_error;
  }
  {
    // Read buffer size range is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-read-size=8",
        "--maximum-read-size=4",
    });
    if (options) return self_test_error;
  }
  {
    // Read size quantum is validated
    auto options = ParseArgsDefault({
        "self-test",
        "--region=r",
        "--minimum-read-size=4",
        "--maximum-read-size=8",
        "--read-quantum=5",
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
  auto const thread_count_arg = gcs_bm::SimpleTimer::SupportPerThreadUsage()
                                    ? "--thread-count=2"
                                    : "--thread-count=1";
  return ParseArgsDefault({
      "self-test",
      "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value(),
      "--region=" + GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID").value(),
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
      "--enabled-apis=JSON",
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
