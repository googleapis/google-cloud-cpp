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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <future>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ObjectNameList = std::vector<std::string>;

// This is basically a smoke test, if the test does not crash it was
// successful. Its main value is when running with the *Sanitizers.
// Synchronization and memory management problems are often revealed by this
// type of test.
class ThreadIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 public:
  static void CreateObjects(std::string const& bucket_name,
                            ObjectNameList const& group,
                            std::string const& contents) {
    // Create our own client so no state is shared with the other threads.
    StatusOr<Client> client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(client);
    for (auto const& object_name : group) {
      (void)client->InsertObject(bucket_name, object_name, contents,
                                 IfGenerationMatch(0));
    }
  }

  static void DeleteObjects(std::string const& bucket_name,
                            ObjectNameList const& group) {
    // Create our own client so no state is shared with the other threads.
    StatusOr<Client> client = MakeIntegrationTestClient();
    ASSERT_STATUS_OK(client);
    for (auto const& object_name : group) {
      (void)client->DeleteObject(bucket_name, object_name);
    }
  }

 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    region_id_ = google::cloud::internal::GetEnv(
                     "GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID")
                     .value_or("");
    ASSERT_FALSE(region_id_.empty());
  }

  std::string project_id_;
  std::string region_id_;
};

/**
 * Divides @p source in to @p count groups of approximately equal size.
 */
std::vector<ObjectNameList> DivideIntoEqualSizedGroups(
    ObjectNameList const& source, unsigned int count) {
  std::vector<ObjectNameList> groups(count);
  std::size_t index = 0;
  for (auto const& name : source) {
    auto group_id = index % count;
    groups[group_id].push_back(name);
  }
  return groups;
}

}  // anonymous namespace

TEST_F(ThreadIntegrationTest, Unshared) {
  std::string bucket_name = MakeRandomBucketName();
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  StatusOr<BucketMetadata> meta = bucket_client->CreateBucketForProject(
      bucket_name, project_id_,
      BucketMetadata()
          .set_storage_class(storage_class::Standard())
          .set_location(region_id_)
          .disable_versioning(),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(bucket_name, meta->name());

  auto constexpr kObjectCount = 2000;
  std::vector<std::string> objects(kObjectCount);
  std::generate(objects.begin(), objects.end(),
                [this] { return MakeRandomObjectName(); });

  auto thread_count = std::thread::hardware_concurrency();
  if (thread_count == 0) thread_count = 4;

  auto const groups = DivideIntoEqualSizedGroups(objects, thread_count);
  std::vector<std::future<void>> tasks(groups.size());
  std::transform(groups.begin(), groups.end(), tasks.begin(),
                 [&](ObjectNameList const& g) {
                   return std::async(std::launch::async,
                                     &ThreadIntegrationTest::CreateObjects,
                                     bucket_name, g, LoremIpsum());
                 });
  for (auto& t : tasks) t.get();
  // Verify at least 1/2 of the objects were successfully created, note that
  // with the default policies an object may be successfully created, but
  // `InsertObject()` returns an error due to retries.
  std::size_t found = 0;
  for (auto& o : client->ListObjects(bucket_name)) {
    if (!o.ok()) break;
    ++found;
  }
  EXPECT_GE(found, kObjectCount / 2);
  std::transform(groups.begin(), groups.end(), tasks.begin(),
                 [&](ObjectNameList const& g) {
                   return std::async(std::launch::async,
                                     &ThreadIntegrationTest::DeleteObjects,
                                     bucket_name, g);
                 });
  for (auto& t : tasks) t.get();

  auto delete_status = bucket_client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(delete_status);
}

class CaptureSendHeaderBackend : public LogBackend {
 public:
  std::vector<std::string> log_lines;

  void Process(LogRecord const& lr) override {
    // Break the records in lines, because we will analyze the output per line.
    std::vector<std::string> lines = absl::StrSplit(lr.message, '\n');
    log_lines.insert(log_lines.end(), lines.begin(), lines.end());
  }

  void ProcessWithOwnership(LogRecord lr) override { Process(lr); }
};

TEST_F(ThreadIntegrationTest, ReuseConnections) {
  auto log_backend = std::make_shared<CaptureSendHeaderBackend>();

  Client client(Options{}.set<TracingComponentsOption>({"raw-client", "http"}));

  std::string bucket_name = MakeRandomBucketName();

  auto id = LogSink::Instance().AddBackend(log_backend);
  StatusOr<BucketMetadata> meta = client.CreateBucketForProject(
      bucket_name, project_id_,
      BucketMetadata()
          .set_storage_class(storage_class::Standard())
          .set_location(region_id_)
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
    StatusOr<ObjectMetadata> insert = client.InsertObject(
        bucket_name, name, LoremIpsum(), IfGenerationMatch(0));
    ASSERT_STATUS_OK(insert);
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
