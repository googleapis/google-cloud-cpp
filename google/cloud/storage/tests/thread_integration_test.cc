// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <future>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ObjectNameList = std::vector<std::string>;

// Initialized in main() below.
char const* flag_project_id;
char const* flag_location;

class ThreadIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 public:
  void CreateObjects(std::string const& bucket_name, ObjectNameList group,
                     std::string contents) {
    // Create our own client so no state is shared with the other threads.
    StatusOr<Client> client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(client);
    for (auto const& object_name : group) {
      StatusOr<ObjectMetadata> meta = client->InsertObject(
          bucket_name, object_name, contents, IfGenerationMatch(0));
      ASSERT_STATUS_OK(meta);
    }
  }

  void DeleteObjects(std::string const& bucket_name, ObjectNameList group) {
    // Create our own client so no state is shared with the other threads.
    StatusOr<Client> client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(client);
    for (auto const& object_name : group) {
      (void)client->DeleteObject(bucket_name, object_name);
    }
  }
};

/**
 * Divides @p source in to @p count groups of approximately equal size.
 */
std::vector<ObjectNameList> DivideIntoEqualSizedGroups(
    ObjectNameList const& source, int count) {
  std::vector<ObjectNameList> groups;
  groups.reserve(count);
  auto div = source.size() / count;
  auto rem = source.size() % count;
  // begin points to the beginning of the next chunk, it is incremented inside
  // the loop by the number of elements.
  std::size_t size;
  for (auto begin = source.begin(); begin != source.end(); begin += size) {
    size = div;
    if (rem != 0) {
      size++;
      rem--;
    }
    auto remaining =
        static_cast<std::size_t>(std::distance(begin, source.end()));
    if (remaining < size) {
      size = remaining;
    }
    auto end = begin;
    std::advance(end, size);
    groups.emplace_back(ObjectNameList(begin, end));
  }
  return groups;
}

}  // anonymous namespace

TEST_F(ThreadIntegrationTest, Unshared) {
  std::string project_id = flag_project_id;
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  StatusOr<BucketMetadata> meta = client->CreateBucketForProject(
      bucket_name, project_id,
      BucketMetadata()
          .set_storage_class(storage_class::Regional())
          .set_location(flag_location)
          .disable_versioning(),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(bucket_name, meta->name());

  constexpr int kObjectCount = 2000;
  std::vector<std::string> objects;
  objects.reserve(kObjectCount);
  std::generate_n(std::back_inserter(objects), kObjectCount,
                  [this] { return MakeRandomObjectName(); });

  unsigned int thread_count = std::thread::hardware_concurrency();
  if (thread_count == 0) {
    thread_count = 4;
  }

  auto groups = DivideIntoEqualSizedGroups(objects, thread_count);
  std::vector<std::future<void>> tasks;
  for (auto const& g : groups) {
    tasks.emplace_back(std::async(std::launch::async,
                                  &ThreadIntegrationTest::CreateObjects, this,
                                  bucket_name, g, LoremIpsum()));
  }
  for (auto& t : tasks) {
    t.get();
  }

  tasks.clear();
  for (auto const& g : groups) {
    tasks.emplace_back(std::async(std::launch::async,
                                  &ThreadIntegrationTest::DeleteObjects, this,
                                  bucket_name, g));
  }
  for (auto& t : tasks) {
    t.get();
  }

  auto delete_status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(delete_status);
  // This is basically a smoke test, if the test does not crash it was
  // successful.
}

class CaptureSendHeaderBackend : public LogBackend {
 public:
  std::vector<std::string> log_lines;

  void Process(LogRecord const& lr) override {
    // Break the records in lines, because we will analyze the output per line.
    std::istringstream is(lr.message);
    std::string line;
    while (std::getline(is, line)) {
      log_lines.emplace_back(line);
    }
  }

  void ProcessWithOwnership(LogRecord lr) override { Process(lr); }
};

TEST_F(ThreadIntegrationTest, ReuseConnections) {
  auto log_backend = std::make_shared<CaptureSendHeaderBackend>();

  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));

  std::string project_id = flag_project_id;
  std::string bucket_name = MakeRandomBucketName();

  auto id = LogSink::Instance().AddBackend(log_backend);
  StatusOr<BucketMetadata> meta = client.CreateBucketForProject(
      bucket_name, project_id,
      BucketMetadata()
          .set_storage_class(storage_class::Regional())
          .set_location(flag_location)
          .disable_versioning(),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(bucket_name, meta->name());

  constexpr int kObjectCount = 100;
  std::vector<std::string> objects;
  objects.reserve(kObjectCount);
  std::generate_n(std::back_inserter(objects), kObjectCount,
                  [this] { return MakeRandomObjectName(); });

  std::vector<std::chrono::steady_clock::duration> create_elapsed;
  std::vector<std::chrono::steady_clock::duration> delete_elapsed;
  for (auto const& name : objects) {
    auto start = std::chrono::steady_clock::now();
    StatusOr<ObjectMetadata> meta = client.InsertObject(
        bucket_name, name, LoremIpsum(), IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    create_elapsed.emplace_back(std::chrono::steady_clock::now() - start);
  }
  for (auto const& name : objects) {
    auto start = std::chrono::steady_clock::now();
    (void)client.DeleteObject(bucket_name, name);
    delete_elapsed.emplace_back(std::chrono::steady_clock::now() - start);
  }
  LogSink::Instance().RemoveBackend(id);
  auto delete_status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(delete_status);

  std::set<std::string> connected;
  std::copy_if(
      log_backend->log_lines.begin(), log_backend->log_lines.end(),
      std::inserter(connected, connected.end()), [](std::string const& line) {
        // libcurl prints established connections using this format:
        //   Connected to <hostname> (<ipaddress>) port <num> (#<connection>)
        // We capturing the different lines in that form tells us how many
        // different connections were used.
        return line.find("== curl(Info): Connected to ") != std::string::npos;
      });
  // We expect that at most 5% of the requests required a new connection,
  // ideally it should be 1 connection, but anything small is acceptable. Recall
  // that we make two requests per connection, so:
  std::size_t max_expected_connections = kObjectCount * 2 * 5 / 100;
  EXPECT_GE(max_expected_connections, connected.size()) << [&log_backend] {
    return std::accumulate(log_backend->log_lines.begin(),
                           log_backend->log_lines.end(), std::string{},
                           [](std::string const& x, std::string const& y) {
                             return x + "\n" + y;
                           });
  }();
  // Zero connections indicates a bug in the test, the client should have
  // connected at least once.
  EXPECT_LT(0U, connected.size());
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project-id> <location (GCP region, e.g us-east1)>\n";
    return 1;
  }

  google::cloud::storage::flag_project_id = argv[1];
  google::cloud::storage::flag_location = argv[2];

  return RUN_ALL_TESTS();
}
