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
#include "google/cloud/internal/throw_delegate.h"
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
simultaneously. The Bucket uses the `REGIONAL` storage class, in a region set
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

constexpr auto kChunkSize = 1 * gcs_bm::kMiB;
constexpr auto kThroughputReportInterval = 4 * gcs_bm::kMiB;

struct Options {
  std::string project_id;
  std::string region;
  std::chrono::seconds duration = std::chrono::seconds(60);
  long object_count = 1000;
  int thread_count = 1;
  std::int64_t object_size = 250 * gcs_bm::kMiB;
  bool enable_connection_pool = true;
  bool enable_xml_api = true;
};

enum OpType { OP_READ, OP_WRITE, OP_CREATE, OP_DELETE, OP_LAST };
struct IterationResult {
  OpType op;
  std::size_t bytes;
  std::chrono::milliseconds elapsed;
};
using TestResult = std::vector<IterationResult>;

char const* ToString(OpType type);
void PrintResult(TestResult const& result);

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    std::string const& bucket_name, Options const& options);

void RunTest(gcs::Client client, std::string const& bucket_name,
             Options const& options,
             std::vector<std::string> const& object_names);

void DeleteAllObjects(gcs::Client client, std::string const& bucket_name,
                      Options const& options,
                      std::vector<std::string> const& object_names);

Options ParseArgs(int argc, char* argv[]);

}  // namespace

int main(int argc, char* argv[]) try {
  Options options = ParseArgs(argc, argv);

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
  if (!options.project_id.empty()) {
    client_options->set_project_id(options.project_id);
  }
  gcs::Client client(*std::move(client_options));

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name =
      gcs_bm::MakeRandomBucketName(generator, "gcs-cpp-throughput-bm-");
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
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Region: " << options.region
            << "\n# Object Count: " << options.object_count
            << "\n# Object Size: " << options.object_size
            << "\n# Object Size (MiB): " << options.object_size / gcs_bm::kMiB
            << "\n# Thread Count: " << options.thread_count
            << "\n# Enable connection pool: " << options.enable_connection_pool
            << "\n# Enable XML API: " << options.enable_xml_api
            << "\n# Build info: " << notes << "\n";

  std::vector<std::string> object_names =
      CreateAllObjects(client, generator, bucket_name, options);
  RunTest(client, bucket_name, options, object_names);
  DeleteAllObjects(client, bucket_name, options, object_names);

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
char const* ToString(OpType type) {
  static char const* kOpTypeNames[] = {"READ", "WRITE", "CREATE", "DELETE",
                                       "LAST"};
  static_assert(OP_LAST + 1 == (sizeof(kOpTypeNames) / sizeof(kOpTypeNames[0])),
                "Mismatched size for OpType names array");
  return kOpTypeNames[type];
}

void PrintResult(TestResult const& result) {
  for (auto const& r : result) {
    std::cout << ToString(r.op) << "," << r.bytes << "," << r.elapsed.count()
              << "\n";
  }
}

TestResult WriteCommon(gcs::Client client, std::string const& bucket_name,
                       std::string const& object_name,
                       std::string const& data_chunk, Options const& options,
                       OpType op_type = OP_WRITE) {
  using std::chrono::milliseconds;
  auto start = std::chrono::steady_clock::now();

  TestResult result;
  result.reserve(options.object_size / kThroughputReportInterval);
  gcs::ObjectWriteStream stream;
  if (options.enable_xml_api) {
    stream = client.WriteObject(bucket_name, object_name, gcs::Fields(""));
  } else {
    stream = client.WriteObject(bucket_name, object_name);
  }
  auto next_report = kThroughputReportInterval;
  for (std::int64_t size = 0; size < options.object_size; size += kChunkSize) {
    stream.write(data_chunk.data(), data_chunk.size());
    if (size > next_report) {
      auto elapsed = std::chrono::steady_clock::now() - start;
      result.emplace_back(
          IterationResult{op_type, static_cast<std::size_t>(size),
                          std::chrono::duration_cast<milliseconds>(elapsed)});
      next_report += kThroughputReportInterval;
    }
  }
  try {
    stream.Close();
    auto elapsed = std::chrono::steady_clock::now() - start;
    result.emplace_back(
        IterationResult{op_type, static_cast<std::size_t>(options.object_size),
                        std::chrono::duration_cast<milliseconds>(elapsed)});
  } catch (std::exception const& ex) {
    std::cerr << ex.what() << "\n";
    auto elapsed = std::chrono::steady_clock::now() - start;
    result.emplace_back(IterationResult{
        op_type, 0, std::chrono::duration_cast<milliseconds>(elapsed)});
  }
  return result;
}

TestResult CreateOnce(gcs::Client client, std::string const& bucket_name,
                      std::string const& object_name,
                      std::string const& data_chunk, Options const& options) {
  return WriteCommon(client, bucket_name, object_name, data_chunk, options,
                     OP_CREATE);
}

TestResult WriteOnce(gcs::Client client, std::string const& bucket_name,
                     std::string const& object_name,
                     std::string const& data_chunk, Options const& options) {
  return WriteCommon(client, bucket_name, object_name, data_chunk, options,
                     OP_WRITE);
}

TestResult ReadOnce(gcs::Client client, std::string const& bucket_name,
                    std::string const& object_name, Options const& options) {
  using std::chrono::milliseconds;
  auto start = std::chrono::steady_clock::now();
  TestResult result;
  result.reserve(options.object_size / kThroughputReportInterval);

  gcs::ObjectReadStream stream;
  if (options.enable_xml_api) {
    stream = client.ReadObject(bucket_name, object_name);
  } else {
    stream = client.ReadObject(bucket_name, object_name,
                               gcs::IfGenerationNotMatch(0));
  }
  std::size_t total_size = 0;
  auto next_report = kThroughputReportInterval;
  char buf[4096];
  while (stream.read(buf, sizeof(buf))) {
    if (stream.gcount() == 0) {
      continue;
    }
    total_size += stream.gcount();
    if (total_size > static_cast<std::size_t>(next_report)) {
      auto elapsed = std::chrono::steady_clock::now() - start;
      result.emplace_back(
          IterationResult{OP_READ, total_size,
                          std::chrono::duration_cast<milliseconds>(elapsed)});
      next_report += kThroughputReportInterval;
    }
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  result.emplace_back(IterationResult{
      OP_READ, total_size, std::chrono::duration_cast<milliseconds>(elapsed)});
  return result;
}

TestResult CreateGroup(gcs::Client client, std::string const& bucket_name,
                       Options const& options, std::vector<std::string> group) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  std::string random_data = gcs_bm::MakeRandomData(generator, kChunkSize);
  TestResult result;
  for (auto const& object_name : group) {
    auto tmp =
        CreateOnce(client, bucket_name, object_name, random_data, options);
    result.insert(result.end(), tmp.begin(), tmp.end());
  }
  return result;
}

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    std::string const& bucket_name, Options const& options) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  std::size_t const max_group_size =
      std::max(options.object_count / options.thread_count, 1L);
  std::cout << "# Creating test objects [" << max_group_size << "]\n";

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
                                    bucket_name, options, std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (!group.empty()) {
    tasks.emplace_back(std::async(std::launch::async, &CreateGroup, client,
                                  bucket_name, options, std::move(group)));
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

TestResult RunTestThread(gcs::Client client, std::string const& bucket_name,
                         Options const& options,
                         std::vector<std::string> const& object_names) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  std::string random_data = gcs_bm::MakeRandomData(generator, kChunkSize);

  std::uniform_int_distribution<std::size_t> object_number_gen(
      0, object_names.size() - 1);
  std::uniform_int_distribution<int> action_gen(0, 99);

  TestResult result;
  result.reserve(static_cast<std::size_t>(options.duration.count()));
  auto deadline = std::chrono::system_clock::now() + options.duration;
  while (std::chrono::system_clock::now() < deadline) {
    auto const& object_name = object_names.at(object_number_gen(generator));
    if (action_gen(generator) < 50) {
      TestResult tmp =
          WriteOnce(client, bucket_name, object_name, random_data, options);
      result.insert(result.end(), tmp.begin(), tmp.end());
    } else {
      TestResult tmp = ReadOnce(client, bucket_name, object_name, options);
      result.insert(result.end(), tmp.begin(), tmp.end());
    }
  }
  return result;
}

void RunTest(gcs::Client client, std::string const& bucket_name,
             Options const& options,
             std::vector<std::string> const& object_names) {
  std::vector<std::future<TestResult>> tasks;
  for (int i = 0; i != options.thread_count; ++i) {
    tasks.emplace_back(std::async(std::launch::async, &RunTestThread, client,
                                  bucket_name, options, object_names));
  }

  for (auto& t : tasks) {
    PrintResult(t.get());
  }
}

TestResult DeleteGroup(gcs::Client client,
                       std::vector<gcs::ObjectMetadata> group) {
  TestResult result;
  for (auto const& o : group) {
    auto start = std::chrono::steady_clock::now();
    auto status = client.DeleteObject(o.bucket(), o.name(),
                                      gcs::Generation(o.generation()));
    if (!status.ok()) {
      google::cloud::internal::ThrowStatus(status);
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    using std::chrono::milliseconds;
    auto ms = std::chrono::duration_cast<milliseconds>(elapsed);
    result.emplace_back(IterationResult{OP_DELETE, 0, ms});
  }
  return result;
}

void DeleteAllObjects(gcs::Client client, std::string const& bucket_name,
                      Options const& options,
                      std::vector<std::string> const& object_names) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  auto const max_group_size =
      std::max(options.object_count / options.thread_count, 1L);

  std::cout << "# Deleting test objects [" << max_group_size << "]\n";
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<TestResult>> tasks;
  std::vector<gcs::ObjectMetadata> group;
  for (auto&& o : client.ListObjects(bucket_name, gcs::Versions(true))) {
    group.emplace_back(std::move(o).value());
    if (group.size() >= static_cast<std::size_t>(max_group_size)) {
      tasks.emplace_back(std::async(std::launch::async, &DeleteGroup, client,
                                    std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (!group.empty()) {
    tasks.emplace_back(
        std::async(std::launch::async, &DeleteGroup, client, std::move(group)));
  }
  // We do not print the latency to delete the objects because we have another
  // benchmark to measure that.
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "# Deleted in " << duration_cast<milliseconds>(elapsed).count()
            << "ms\n";
}

Options ParseArgs(int argc, char* argv[]) {
  Options options;
  bool wants_help = false;
  bool wants_description = false;
  std::vector<gcs_bm::OptionDescriptor> desc{
      {"--help", "print usage information",
       [&wants_help](std::string const& v) { wants_help = true; }},
      {"--description", "print benchmark description",
       [&wants_description](std::string const& v) {
         wants_description = true;
       }},
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
      {"--thread-count", "set the number of threads in the benchmark",
       [&options](std::string const& val) {
         options.thread_count = std::stoi(val);
       }},
      {"--enable-connection-pool", "enable the client library connection pool",
       [&options](std::string const& val) {
         options.enable_connection_pool = gcs_bm::ParseBoolean(val, true);
       }},
      {"--enable-xml-api", "enable the XML API for the benchmark",
       [&options](std::string const& val) {
         options.enable_xml_api = gcs_bm::ParseBoolean(val, true);
       }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--region", "use the given region for the benchmark",
       [&options](std::string const& val) { options.region = val; }},
  };
  auto usage = gcs_bm::BuildUsage(desc, argv[0]);

  auto unparsed = gcs_bm::OptionsParse(desc, {argv, argv + argc});
  if (wants_help) {
    std::cout << usage << "\n";
  }

  if (wants_description) {
    std::cout << kDescription << "\n";
  }

  if (unparsed.size() > 2U) {
    std::ostringstream os;
    os << "Unknown arguments or options\n" << usage << "\n";
    throw std::runtime_error(std::move(os).str());
  }
  if (unparsed.size() == 2U) {
    options.region = unparsed[1];
  }
  if (options.region.empty()) {
    std::ostringstream os;
    os << "Missing value for --region option" << usage << "\n";
    throw std::runtime_error(std::move(os).str());
  }

  return options;
}

}  // namespace
