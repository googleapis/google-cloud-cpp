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
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/format_rfc3339.h"
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

enum OpType { OP_READ, OP_WRITE, OP_CREATE, OP_DELETE, OP_LAST };
struct IterationResult {
  OpType op;
  bool success;
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

}  // namespace

int main(int argc, char* argv[]) try {
  Options options;
  options.ParseArgs(argc, argv);

  if (not google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").has_value()) {
    std::cerr << "GOOGLE_CLOUD_PROJECT environment variable must be set"
              << std::endl;
    return 1;
  }

  gcs::ClientOptions client_options;
  if (not options.enable_connection_pool) {
    client_options.set_connection_pool_size(0);
  }
  gcs::Client client(client_options);

  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto bucket_name = MakeRandomBucketName(generator);
  auto meta =
      client.CreateBucket(bucket_name,
                          gcs::BucketMetadata()
                              .set_storage_class(gcs::storage_class::Regional())
                              .set_location(options.region),
                          gcs::PredefinedAcl("private"),
                          gcs::PredefinedDefaultObjectAcl("projectPrivate"),
                          gcs::Projection("full"));
  std::cout << "# Running test on bucket: " << meta.name() << std::endl;
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
            << "\n# Build info: " << notes << std::endl;

  std::vector<std::string> object_names =
      CreateAllObjects(client, generator, bucket_name, options);
  RunTest(client, bucket_name, options, object_names);
  DeleteAllObjects(client, bucket_name, options, object_names);
  std::cout << "# Deleting " << bucket_name << std::endl;
  client.DeleteBucket(bucket_name);

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
std::string Basename(std::string const& path) {
  // Sure would be nice to be using C++17 where std::filesytem is a thing.
#if _WIN32
  return path.substr(path.find_last_of('\\') + 1);
#else
  return path.substr(path.find_last_of('/') + 1);
#endif  // _WIN32
}

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

char const* ToString(OpType type) {
  static char const* kOpTypeNames[] = {"READ", "WRITE", "CREATE", "DELETE",
                                       "LAST"};
  static_assert(OP_LAST + 1 == (sizeof(kOpTypeNames) / sizeof(kOpTypeNames[0])),
                "Mismatched size for OpType names array");
  return kOpTypeNames[type];
}

void PrintResult(TestResult const& result) {
  for (auto const& r : result) {
    std::cout << ToString(r.op) << "," << std::boolalpha << r.success << ","
              << r.elapsed.count() << "\n";
  }
}

IterationResult WriteCommon(gcs::Client client, std::string const& bucket_name,
                            std::string const& object_name,
                            std::string const& random_data,
                            Options const& options, OpType op_type) {
  auto start = std::chrono::steady_clock::now();
  if (options.enable_xml_api) {
    (void)client.InsertObject(bucket_name, object_name, random_data,
                              gcs::Fields(""));
  } else {
    (void)client.InsertObject(bucket_name, object_name, random_data);
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  using std::chrono::milliseconds;
  return IterationResult{op_type, true,
                         std::chrono::duration_cast<milliseconds>(elapsed)};
}

IterationResult CreateOnce(gcs::Client const& client,
                           std::string const& bucket_name,
                           std::string const& object_name,
                           std::string const& data_chunk,
                           Options const& options) {
  return WriteCommon(client, bucket_name, object_name, data_chunk, options,
                     OP_CREATE);
}

IterationResult WriteOnce(gcs::Client const& client,
                          std::string const& bucket_name,
                          std::string const& object_name,
                          std::string const& data_chunk,
                          Options const& options) {
  return WriteCommon(client, bucket_name, object_name, data_chunk, options,
                     OP_WRITE);
}

IterationResult ReadOnce(gcs::Client client, std::string const& bucket_name,
                         std::string const& object_name,
                         Options const& options) {
  auto start = std::chrono::steady_clock::now();
  gcs::ObjectReadStream stream;
  if (options.enable_xml_api) {
    stream = client.ReadObject(bucket_name, object_name);
  } else {
    stream = client.ReadObject(bucket_name, object_name,
                               gcs::IfGenerationNotMatch(0));
  }
  std::size_t total_size = 0;
  while (not stream.eof()) {
    char buf[4096];
    stream.read(buf, sizeof(buf));
    total_size += stream.gcount();
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  using std::chrono::milliseconds;
  auto ms = std::chrono::duration_cast<milliseconds>(elapsed);
  return IterationResult{OP_READ, (total_size == kBlobSize), ms};
}

TestResult CreateGroup(gcs::Client client, std::string const& bucket_name,
                       Options const& options, std::vector<std::string> group) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  std::string random_data = MakeRandomData(generator, kBlobSize);
  TestResult result;
  for (auto const& object_name : group) {
    result.emplace_back(
        CreateOnce(client, bucket_name, object_name, random_data, options));
  }
  return result;
}

std::vector<std::string> CreateAllObjects(
    gcs::Client client, google::cloud::internal::DefaultPRNG& gen,
    std::string const& bucket_name, Options const& options) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  auto const max_group_size =
      std::max(options.object_count / options.thread_count, 1L);
  std::cout << "# Creating test objects [" << max_group_size << "] "
            << std::endl;

  // Generate the list of object names.
  std::vector<std::string> object_names;
  object_names.reserve(options.object_count);
  for (long c = 0; c != options.object_count; ++c) {
    object_names.emplace_back(MakeRandomObjectName(gen));
  }

  // Split the objects in more or less equally sized groups, launch a thread
  // to create the objects in each group.
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<TestResult>> tasks;
  std::vector<std::string> group;
  group.reserve(max_group_size);
  for (auto const& o : object_names) {
    group.push_back(o);
    if (group.size() >= static_cast<std::size_t>(max_group_size)) {
      tasks.emplace_back(std::async(std::launch::async, &CreateGroup, client,
                                    bucket_name, options, std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (not group.empty()) {
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
            << "ms" << std::endl;
  return object_names;
}

TestResult RunTestThread(gcs::Client const& client,
                         std::string const& bucket_name, Options const& options,
                         std::vector<std::string> object_names) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  std::string random_data = MakeRandomData(generator, kBlobSize);

  std::uniform_int_distribution<std::size_t> object_number_gen(
      0, object_names.size() - 1);
  std::uniform_int_distribution<int> action_gen(0, 99);

  TestResult result;
  // Reserve memory assuming the iterations take around 200ms. If the assumption
  // is too small, all that happens is some extra allocations. If the allocation
  // is too big we may have several MiB of over allocated memory.
  result.reserve(static_cast<std::size_t>(5 * options.duration.count()));
  auto deadline = std::chrono::system_clock::now() + options.duration;
  while (std::chrono::system_clock::now() < deadline) {
    auto const& object_name = object_names.at(object_number_gen(generator));
    if (action_gen(generator) < 50) {
      result.emplace_back(
          WriteOnce(client, bucket_name, object_name, random_data, options));
    } else {
      result.emplace_back(ReadOnce(client, bucket_name, object_name, options));
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
                       std::vector<gcs::ObjectMetadata> const& group) {
  TestResult result;
  for (auto const& o : group) {
    auto start = std::chrono::steady_clock::now();
    client.DeleteObject(o.bucket(), o.name(), gcs::Generation(o.generation()));
    auto elapsed = std::chrono::steady_clock::now() - start;
    using std::chrono::milliseconds;
    auto ms = std::chrono::duration_cast<milliseconds>(elapsed);
    result.emplace_back(IterationResult{OP_DELETE, true, ms});
  }
  return result;
}

void DeleteAllObjects(gcs::Client client, std::string const& bucket_name,
                      Options const& options,
                      std::vector<std::string> const& object_names) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  std::size_t const max_group_size =
      std::max(object_names.size() / options.thread_count, std::size_t(1));

  std::cout << "# Deleting test objects [" << max_group_size << "]"
            << std::endl;
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<TestResult>> tasks;
  std::vector<gcs::ObjectMetadata> group;
  for (auto const& o : client.ListObjects(bucket_name, gcs::Versions(true))) {
    group.push_back(o);
    if (group.size() >= max_group_size) {
      tasks.emplace_back(std::async(std::launch::async, &DeleteGroup, client,
                                    std::move(group)));
      group = {};  // after a move, must assign to guarantee it is valid.
    }
  }
  if (not group.empty()) {
    tasks.emplace_back(
        std::async(std::launch::async, &DeleteGroup, client, std::move(group)));
  }
  for (auto& t : tasks) {
    PrintResult(t.get());
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "# Deleted in " << duration_cast<milliseconds>(elapsed).count()
            << "ms" << std::endl;
}

void Options::ParseArgs(int& argc, char* argv[]) {
  region = ConsumeArg(argc, argv, "region");
}

std::string Options::ConsumeArg(int& argc, char* argv[], char const* arg_name) {
  std::string const duration = "--duration=";
  std::string const object_count = "--object-count=";
  std::string const thread_count = "--thread-count=";
  std::string const enable_connection_pool = "--enable-connection-pool=";
  std::string const enable_xml_api = "--enable-xml-api=";

  std::string const usage = R""(
[options] <region>
The options are:
    --help: produce this message.
    --duration (in seconds): for how long should the test run.
    --object-count: the number of objects to use in the benchmark.
    --thread-count: the number of threads to use in the benchmark.
    --enable-connection-pool: reuse connections across requests.
    --enable-xml-api: configure read+write operations to use XML API.

    region: a Google Cloud Storage region where all the objects used in this
       test will be located.
)"";

  std::string error;
  while (argc >= 2) {
    std::string argument(argv[1]);
    std::copy(argv + 2, argv + argc, argv + 1);
    argc--;
    if (argument == "--help") {
    } else if (0 == argument.rfind(duration, 0)) {
      std::string val = argument.substr(duration.size());
      this->duration = std::chrono::seconds(std::stoi(val));
    } else if (0 == argument.rfind(object_count, 0)) {
      auto arg = argument.substr(object_count.size());
      auto val = std::stol(arg);
      if (val <= 0) {
        error = "Invalid object-count argument (" + arg + ")";
        break;
      }
      this->object_count = val;
    } else if (0 == argument.rfind(thread_count, 0)) {
      auto arg = argument.substr(object_count.size());
      auto val = std::stoi(arg);
      if (val <= 0) {
        error = "Invalid thread-count argument (" + arg + ")";
        break;
      }
      this->thread_count = val;
    } else if (0 == argument.rfind(enable_connection_pool, 0)) {
      auto arg = argument.substr(enable_connection_pool.size());
      if (arg == "true" or arg == "yes" or arg == "1") {
        this->enable_connection_pool = true;
      } else if (arg == "false" or arg == "no" or arg == "0") {
        this->enable_connection_pool = false;
      } else {
        error = "Invalid enable-connection-pool argument (" + arg + ")";
        break;
      }
    } else if (0 == argument.rfind(enable_xml_api, 0)) {
      auto arg = argument.substr(enable_xml_api.size());
      if (arg == "true" or arg == "yes" or arg == "1") {
        this->enable_xml_api = true;
      } else if (arg == "false" or arg == "no" or arg == "0") {
        this->enable_xml_api = false;
      } else {
        error = "Invalid enable-xml-api argument (" + arg + ")";
        break;
      }
    } else {
      return argument;
    }
  }
  std::ostringstream os;
  os << "Missing argument " << arg_name << "\n";
  os << "Usage: " << Basename(argv[0]) << usage << std::endl;
  throw std::runtime_error(os.str());
}

}  // namespace
