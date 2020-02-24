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
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/client.h"
#include <future>
#include <iomanip>
#include <sstream>

namespace {
namespace gcs = google::cloud::storage;
namespace gcs_bm = google::cloud::storage_benchmarks;

char const kDescription[] = R"""(
A throughput benchmark for the Google Cloud Storage C++ client library.

This program first creates a Bucket that will contain all the Objects used in
the test.  The Bucket is deleted at the end of the test. The name of the Bucket
is selected at random, so multiple instances of this benchmark can run
simultaneously. The Bucket uses the `STANDARD` storage class, in a region set
via the command-line.

After creating this Bucket the program creates a number of objects, all the
objects have the same contents, but the contents are generated at random.

The size of the objects can be configured in the command-line, but they are
typically 250MiB is size.  The program reports the time it takes to upload the
first 10 MiB, the first 20 MiB, the first 30 MiB, and so forth until the total
size of the object is uploaded.

Once the object creation phase is completed, the program starts N threads, each
thread executes a simple loop:
- Pick one of the objects at random, with equal probability for each Object.
- Pick, with equal probably, an action (`read` or `write`) at random.
- If the action was `write` then write to the object, capturing throughput
  information, which is reported when the thread finishes running.
- If the action was `read` then read the object. Capture the time taken to
  read the first 10 MiB, the first 20 MiB, and so forth until the full
  object is read.

The loop runs for a prescribed number of seconds. At the end of the loop the
program prints the captured performance data.

Then the program removes all the objects in the bucket and reports the time
taken to delete each one.

A helper script in this directory can generate pretty graphs from the outut of
this program.
)""";

struct Options {
  std::string project_id;
  std::string region;
  std::string bucket_name;
  std::chrono::seconds duration = std::chrono::seconds(60);
  long minimum_sample_count = 0;
  long maximum_sample_count = std::numeric_limits<long>::max();
  long object_count = 1000;
  int thread_count = 1;
  std::int64_t object_size = 250 * gcs_bm::kMiB;
  std::int64_t chunk_size = 32 * gcs_bm::kMiB;
  std::int64_t minimum_read_size = 0;
  std::int64_t maximum_read_size = 0;
  bool enable_connection_pool = true;
  bool enable_xml_api = true;
  bool create_bucket = true;
  bool delete_bucket = true;
  bool create_objects = true;
  bool upload_objects = true;
  bool download_objects = true;
};

enum class OpType { OP_READ, OP_WRITE, OP_CREATE };
enum class ApiName { API_JSON, API_XML };
struct IterationResult {
  OpType op;
  ApiName api;
  std::size_t bytes;
  std::chrono::microseconds elapsed;
};
using TestResult = std::vector<IterationResult>;

char const* ToString(OpType type);
char const* ToString(ApiName api);
void PrintResult(TestResult const& result);

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    Options const& options);
std::vector<std::string> GetObjectNames(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    Options const& options);

void RunTest(gcs::Client client, Options const& options,
             std::vector<std::string> const& object_names);

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
  if (!options->enable_connection_pool) {
    client_options->set_connection_pool_size(0);
  }
  if (!options->project_id.empty()) {
    client_options->set_project_id(options->project_id);
  }
  gcs::Client client(*std::move(client_options));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  if (options->bucket_name.empty()) {
    options->bucket_name =
        gcs_bm::MakeRandomBucketName(generator, "gcs-cpp-throughput-bm-");
  }
  if (options->create_bucket) {
    std::cout << "# Creating bucket to run the test, region: "
              << options->region << "\n";
    auto meta =
        client
            .CreateBucket(options->bucket_name,
                          gcs::BucketMetadata()
                              .set_storage_class(gcs::storage_class::Standard())
                              .set_location(options->region),
                          gcs::PredefinedAcl("private"),
                          gcs::PredefinedDefaultObjectAcl("projectPrivate"),
                          gcs::Projection("full"))
            .value();
    std::cout << "# Running test on bucket: " << options->bucket_name
              << "\n# Bucket Region: " << meta.location()
              << "\n# Bucket Location Type: " << meta.location_type()
              << "\n# Bucket Storage Class: " << meta.storage_class() << "\n";
  }

  std::string notes = google::cloud::storage::version_string() + ";" +
                      google::cloud::internal::compiler() + ";" +
                      google::cloud::internal::compiler_flags();
  std::transform(notes.begin(), notes.end(), notes.begin(),
                 [](char c) { return c == '\n' ? ';' : c; });
  std::cout << "# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Region: " << options->region
            << "\n# Bucket Name: " << options->bucket_name
            << "\n# Test Duration: " << options->duration.count() << "s"
            << "\n# Minimum Sample Count: " << options->minimum_sample_count
            << "\n# Maximum Sample Count: " << options->maximum_sample_count
            << "\n# Object Count: " << options->object_count
            << "\n# Thread Count: " << options->thread_count
            << "\n# Object Size: " << gcs_bm::FormatSize(options->object_size)
            << "\n# Chunk Size: " << gcs_bm::FormatSize(options->chunk_size)
            << "\n# Minimum Read Size: "
            << gcs_bm::FormatSize(options->minimum_read_size)
            << "\n# Maximum Read Size: "
            << gcs_bm::FormatSize(options->maximum_read_size) << std::boolalpha
            << "\n# Enable connection pool: " << options->enable_connection_pool
            << "\n# Enable XML API: " << options->enable_xml_api
            << "\n# Create Bucket: " << options->create_bucket
            << "\n# Delete Bucket: " << options->delete_bucket
            << "\n# Create Objects: " << options->create_objects
            << "\n# Upload Objects: " << options->upload_objects
            << "\n# Download Objects: " << options->download_objects
            << "\n# Build info: " << notes
            << "\n# OpType,ApiName,Bytes,ElapsedTime(us)" << std::endl;

  std::vector<std::string> object_names;
  if (options->create_objects) {
    object_names = CreateAllObjects(client, generator, *options);
  } else {
    object_names = GetObjectNames(client, generator, *options);
  }
  RunTest(client, *options, object_names);

  if (options->delete_bucket) {
    gcs_bm::DeleteAllObjects(client, options->bucket_name,
                             options->thread_count);
    std::cout << "# Deleting " << options->bucket_name << "\n";
    auto status = client.DeleteBucket(options->bucket_name);
    if (!status.ok()) {
      std::cerr << "# Error deleting bucket, status=" << status << "\n";
      return 1;
    }
  }

  return 0;
}

namespace {
char const* ToString(OpType type) {
  switch (type) {
    case OpType::OP_CREATE:
      return "CREATE";
    case OpType::OP_READ:
      return "READ";
    case OpType::OP_WRITE:
      return "WRITE";
  }
  return "";
}

char const* ToString(ApiName api) {
  switch (api) {
    case ApiName::API_JSON:
      return "JSON";
    case ApiName::API_XML:
      return "XML";
  }
  return "";
}

void PrintResult(TestResult const& result) {
  for (auto const& r : result) {
    std::cout << ToString(r.op) << ',' << ToString(r.api) << ',' << r.bytes
              << ',' << r.elapsed.count() << "\n";
  }
}

TestResult WriteCommon(gcs::Client client, std::string const& object_name,
                       std::string const& data_chunk, Options const& options,
                       OpType op_type, ApiName api) {
  using std::chrono::microseconds;
  auto start = std::chrono::steady_clock::now();

  TestResult result;

  gcs::ObjectWriteStream stream = client.WriteObject(
      options.bucket_name, object_name,
      api == ApiName::API_JSON ? gcs::Fields{} : gcs::Fields{""});
  for (std::int64_t size = 0; size < options.object_size;
       size += data_chunk.size()) {
    auto const n = (std::min<std::streamsize>)(data_chunk.size(),
                                               options.object_size - size);
    stream.write(data_chunk.data(), n);
    if (stream.bad()) break;
  }

  stream.Close();
  // ObjectWriteStream has no status() method. However, if an error occurred
  // during Close(), the status with the error message is returned by
  // metadata().
  auto status = stream.metadata();
  if (!status.ok()) {
    std::cerr << status.status().message() << "\n";
    auto elapsed = std::chrono::steady_clock::now() - start;
    result.emplace_back(IterationResult{
        op_type, api, 0, std::chrono::duration_cast<microseconds>(elapsed)});
  } else {
    auto elapsed = std::chrono::steady_clock::now() - start;
    result.emplace_back(IterationResult{
        op_type, api, static_cast<std::size_t>(options.object_size),
        std::chrono::duration_cast<microseconds>(elapsed)});
  }
  return result;
}

TestResult CreateOnce(gcs::Client client, std::string const& object_name,
                      std::string const& data_chunk, Options const& options) {
  return WriteCommon(client, object_name, data_chunk, options,
                     OpType::OP_CREATE, ApiName::API_XML);
}

TestResult WriteOnce(gcs::Client client, std::string const& object_name,
                     std::string const& data_chunk, Options const& options,
                     ApiName api) {
  return WriteCommon(client, object_name, data_chunk, options, OpType::OP_WRITE,
                     api);
}

TestResult ReadOnce(gcs::Client client, std::string const& object_name,
                    Options const& options, ApiName api, gcs::ReadRange range) {
  using std::chrono::microseconds;
  auto start = std::chrono::steady_clock::now();
  TestResult result;

  gcs::ObjectReadStream stream =
      client.ReadObject(options.bucket_name, object_name,
                        api == ApiName::API_JSON ? gcs::IfGenerationNotMatch{0}
                                                 : gcs::IfGenerationNotMatch{},
                        std::move(range));
  std::size_t total_size = 0;
  std::vector<char> buffer(options.chunk_size);
  do {
    stream.read(buffer.data(), buffer.size());
    total_size += stream.gcount();
  } while (stream.good());
  auto elapsed = std::chrono::steady_clock::now() - start;
  result.emplace_back(
      IterationResult{OpType::OP_READ, api, total_size,
                      std::chrono::duration_cast<microseconds>(elapsed)});
  return result;
}

std::string CreateChunk(google::cloud::internal::DefaultPRNG& generator,
                        std::size_t desired_size) {
  std::string const random_data =
      gcs_bm::MakeRandomData(generator, gcs_bm::kMiB);
  std::string chunk;
  chunk.reserve(desired_size);
  while (chunk.size() < desired_size) {
    chunk.append(random_data);
  }
  chunk.resize(desired_size);
  return chunk;
}

TestResult CreateGroup(gcs::Client client, Options const& options,
                       std::vector<std::string> group) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto const chunk = CreateChunk(generator, options.chunk_size);
  TestResult result;
  for (auto const& object_name : group) {
    auto tmp = CreateOnce(client, object_name, chunk, options);
    result.insert(result.end(), tmp.begin(), tmp.end());
  }
  return result;
}

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    Options const& options) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  std::size_t const max_group_size =
      std::max(options.object_count / options.thread_count, 1L);
  std::cout << "# Creating test objects [" << max_group_size << "]"
            << std::endl;

  // Generate the list of object names.
  std::vector<std::string> object_names;
  object_names.reserve(options.object_count);
  for (long c = 0; c != options.object_count; ++c) {
    object_names.emplace_back(gcs_bm::MakeRandomObjectName(gen));
  }

  // Split the objects in more or less equally sized groups, launch a thread
  // to create the objects in each group.
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<TestResult>> tasks;
  std::vector<std::string> group;
  for (auto const& o : object_names) {
    group.push_back(o);
    if (group.size() >= max_group_size) {
      tasks.emplace_back(std::async(std::launch::async, &CreateGroup, client,
                                    options, std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (!group.empty()) {
    tasks.emplace_back(std::async(std::launch::async, &CreateGroup, client,
                                  options, std::move(group)));
    group = {};  // after a move, must assign to guarantee it is valid.
  }
  // Wait for the threads to finish.
  for (auto& t : tasks) {
    PrintResult(t.get());
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "# Created in " << duration_cast<milliseconds>(elapsed).count()
            << "ms\n";
  return object_names;
}

std::vector<std::string> GetObjectNames(gcs::Client client,
                                        google::cloud::internal::DefaultPRNG&,
                                        Options const& options) {
  std::vector<std::string> object_names;
  object_names.reserve(options.object_count);
  for (auto const& o : client.ListObjects(options.bucket_name)) {
    if (!o) break;
    object_names.push_back(o->name());
    if (object_names.size() >= static_cast<std::size_t>(options.object_count)) {
      break;
    }
  }
  return object_names;
}

TestResult RunTestThread(gcs::Client client, Options const& options,
                         std::vector<std::string> const& object_names) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto const chunk = CreateChunk(generator, options.chunk_size);

  std::uniform_int_distribution<std::size_t> object_number_gen(
      0, object_names.size() - 1);
  std::uniform_int_distribution<std::int64_t> range_gen(
      options.minimum_read_size, options.maximum_read_size);
  std::bernoulli_distribution action_gen;
  std::uniform_int_distribution<int> api_gen(0, 1);
  if (!options.enable_xml_api) {
    api_gen = std::uniform_int_distribution<int>(0, 0);
  }

  TestResult result;
  result.reserve(static_cast<std::size_t>(options.duration.count()));
  auto deadline = std::chrono::steady_clock::now() + options.duration;
  long iteration_count = 0;
  for (auto start = std::chrono::steady_clock::now();
       iteration_count < options.maximum_sample_count &&
       (iteration_count < options.minimum_sample_count || start < deadline);
       start = std::chrono::steady_clock::now(), ++iteration_count) {
    auto const& object_name = object_names.at(object_number_gen(generator));
    auto const api =
        api_gen(generator) == 0 ? ApiName::API_JSON : ApiName::API_XML;
    if (action_gen(generator) && options.upload_objects) {
      TestResult tmp = WriteOnce(client, object_name, chunk, options, api);
      result.insert(result.end(), tmp.begin(), tmp.end());
      continue;
    }
    if (options.download_objects) {
      auto range = gcs::ReadRange();
      if (options.maximum_read_size != 0) {
        range = gcs::ReadRange(0, range_gen(generator));
      }
      TestResult tmp =
          ReadOnce(client, object_name, options, api, std::move(range));
      result.insert(result.end(), tmp.begin(), tmp.end());
    }
  }
  return result;
}

void RunTest(gcs::Client client, Options const& options,
             std::vector<std::string> const& object_names) {
  std::vector<std::future<TestResult>> tasks;
  for (int i = 0; i != options.thread_count; ++i) {
    tasks.emplace_back(std::async(std::launch::async, &RunTestThread, client,
                                  options, object_names));
  }

  for (auto& t : tasks) {
    PrintResult(t.get());
  }
}

google::cloud::StatusOr<Options> ParseArgs(int argc, char* argv[]) {
  Options options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<gcs_bm::OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const&) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const&) { wants_description = true; }},
      {"--duration", "set the total execution time for the benchmark",
       [&options](std::string const& val) {
         options.duration = gcs_bm::ParseDuration(val);
       }},
      {"--object-count", "set the number of objects created by the benchmark",
       [&options](std::string const& val) {
         options.object_count = std::stoi(val);
       }},
      {"--object-size", "size of the objects created by the benchmark",
       [&options](std::string const& val) {
         options.object_size = gcs_bm::ParseSize(val);
       }},
      {"--chunk-size", "size of the chunk used for transfers",
       [&options](std::string const& val) {
         options.chunk_size = gcs_bm::ParseSize(val);
       }},
      {"--minimum-read-size", "minimum read size",
       [&options](std::string const& val) {
         options.minimum_read_size = gcs_bm::ParseSize(val);
       }},
      {"--maximum-read-size",
       "maximum read size, set to 0 to read the full object each time",
       [&options](std::string const& val) {
         options.maximum_read_size = gcs_bm::ParseSize(val);
       }},
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--enable-connection-pool", "enable the client library connection pool",
       [&options](std::string const& val) {
         options.enable_connection_pool =
             gcs_bm::ParseBoolean(val).value_or(true);
       }},
      {"--enable-xml-api", "enable the XML API for the benchmark",
       [&options](std::string const& val) {
         options.enable_xml_api = gcs_bm::ParseBoolean(val).value_or(true);
       }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--region", "use this region if the benchmark creates a bucket",
       [&options](std::string const& val) { options.region = val; }},
      {"--bucket-name", "use an existing bucket to run the benchmark",
       [&options](std::string const& val) { options.bucket_name = val; }},
      {"--create-bucket", "create a new bucket for the benchmark",
       [&options](std::string const& val) {
         options.create_bucket = gcs_bm::ParseBoolean(val).value_or(true);
       }},
      {"--delete-bucket", "delete the bucket after running the benchmark",
       [&options](std::string const& val) {
         options.delete_bucket = gcs_bm::ParseBoolean(val).value_or(true);
       }},
      {"--create-objects", "run the benchmark that creates new objects",
       [&options](std::string const& val) {
         options.create_objects = gcs_bm::ParseBoolean(val).value_or(true);
       }},
      {"--upload-objects", "run the benchmark that uploads objects",
       [&options](std::string const& val) {
         options.upload_objects = gcs_bm::ParseBoolean(val).value_or(true);
       }},
      {"--download-objects", "run the benchmark that downloads objects",
       [&options](std::string const& val) {
         options.download_objects = gcs_bm::ParseBoolean(val).value_or(true);
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

  auto unparsed = gcs_bm::OptionsParse(desc, {argv, argv + argc});
  if (wants_help) {
    std::cout << usage << "\n";
    std::exit(0);
  }

  if (wants_description) {
    std::cout << kDescription << "\n";
    std::exit(0);
  }

  if (unparsed.size() > 1) {
    std::ostringstream os;
    os << "Unknown arguments or options:";
    for (std::size_t i = 1; i != unparsed.size(); ++i) {
      os << " " << unparsed[i];
    }
    os << "\n" << usage << "\n";
    return google::cloud::Status{google::cloud::StatusCode::kInvalidArgument,
                                 std::move(os).str()};
  }
  if (options.region.empty() && options.bucket_name.empty()) {
    std::ostringstream os;
    os << "Both --region and --bucket-name options were missing, you must set"
       << " at least one of them:\n"
       << usage << "\n";
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

}  // namespace
