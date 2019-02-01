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

#include "google/cloud/log.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using google::cloud::storage::testing::CountMatchingEntities;
using google::cloud::storage::testing::TestPermanentFailure;
using ::testing::HasSubstr;

/// Store the project and instance captured from the command-line arguments.
class ObjectTestEnvironment : public ::testing::Environment {
 public:
  ObjectTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
};

std::string ObjectTestEnvironment::project_id_;
std::string ObjectTestEnvironment::bucket_name_;

class ObjectInsertIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(ObjectInsertIntegrationTest, SimpleInsertWithNonUrlSafeName) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      DisableCrc32cChecksum(true), DisableMD5Hash(true));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertWithNonUrlSafeName) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, MultipartInsertWithNonUrlSafeName) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertWithMD5) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      MD5HashValue("96HF9K981B+JfoQuTVnyCg=="));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertWithComputedMD5) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      MD5HashValue(ComputeMD5Hash(expected)));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertWithMD5) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""),
      MD5HashValue("96HF9K981B+JfoQuTVnyCg=="));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertWithMetadata) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata()
                             .upsert_metadata("test-key", "test-value")
                             .set_content_type("text/plain")));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  EXPECT_TRUE(meta->has_metadata("test-key"));
  EXPECT_EQ("test-value", meta->metadata("test-key"));
  EXPECT_EQ("text/plain", meta->content_type());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertPredefinedAclAuthenticatedRead) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::AuthenticatedRead(), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertPredefinedAclBucketOwnerFullControl) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_TRUE(bucket.ok()) << "status=" << bucket.status();
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerFullControl(), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertPredefinedAclBucketOwnerRead) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_TRUE(bucket.ok()) << "status=" << bucket.status();
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerRead(), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertPredefinedAclPrivate) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::Private(), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertPredefinedAclProjectPrivate) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::ProjectPrivate(), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertPredefinedAclPublicRead) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::PublicRead(), Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertPredefinedAclAuthenticatedRead) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::AuthenticatedRead(), Fields(""));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest,
       XmlInsertPredefinedAclBucketOwnerFullControl) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_TRUE(bucket.ok()) << "status=" << bucket.status();
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerFullControl(), Fields(""));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertPredefinedAclBucketOwnerRead) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_TRUE(bucket.ok()) << "status=" << bucket.status();
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerRead(), Fields(""));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertPredefinedAclPrivate) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::Private(), Fields(""));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertPredefinedAclProjectPrivate) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::ProjectPrivate(), Fields(""));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, XmlInsertPredefinedAclPublicRead) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::PublicRead(), Fields(""));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name, object_name, Projection::Full());
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();
  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

/**
 * @test Verify that `QuotaUser` inserts the correct query parameter.
 *
 * Testing for `QuotaUser` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_F(ObjectInsertIntegrationTest, InsertWithQuotaUser) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(opts.ok()) << "status=" << opts.status();
  Client client((*std::move(opts))
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), QuotaUser("test-quota-user"));
  ASSERT_TRUE(insert_meta.ok()) << "status=" << insert_meta.status();

  LogSink::Instance().RemoveBackend(id);

  // Create the regular expression we want to match.
  std::regex re = [&bucket_name] {
    std::string regex = ".* POST .*";
    regex += "/b/" + bucket_name + "/o";
    regex += ".*quotaUser=test-quota-user.*";
    return std::regex(regex, std::regex_constants::egrep);
  }();

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [&re](std::string const& line) { return std::regex_match(line, re); });
  EXPECT_LT(0, count);

  auto status = client.DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

/**
 * @test Verify that `userIp` inserts the correct query parameter.
 *
 * Testing for `userIp` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_F(ObjectInsertIntegrationTest, InsertWithUserIp) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(opts.ok()) << "status=" << opts.status();
  Client client((*std::move(opts))
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), UserIp("127.0.0.1"));
  ASSERT_TRUE(insert_meta.ok()) << "status=" << insert_meta.status();

  LogSink::Instance().RemoveBackend(id);

  // Create the regular expression we want to match.
  std::regex re = [&bucket_name] {
    std::string regex = ".* POST .*";
    regex += "/b/" + bucket_name + "/o";
    regex += ".*userIp=127\\.0\\.0\\.1.*";
    return std::regex(regex, std::regex_constants::egrep);
  }();

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [&re](std::string const& line) { return std::regex_match(line, re); });
  EXPECT_LT(0, count);

  auto status = client.DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

/**
 * @test Verify that `userIp` inserts a query parameter.
 *
 * Testing for `userIp` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_F(ObjectInsertIntegrationTest, InsertWithUserIpBlank) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_TRUE(opts.ok()) << "status=" << opts.status();
  Client client((*std::move(opts))
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Make sure at least one connection was created before we run the test, the
  // IP address can only be obtained once the first request to a given endpoint
  // is completed.
  {
    auto seed_object_name = MakeRandomObjectName();
    auto insert =
        client.InsertObject(bucket_name, seed_object_name, LoremIpsum());
    ASSERT_TRUE(insert.ok()) << "status=" << insert.status();
    auto status = client.DeleteObject(bucket_name, seed_object_name);
    ASSERT_TRUE(status.ok()) << "status=" << status;
  }

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0), UserIp(""));
  ASSERT_TRUE(insert_meta.ok()) << "status=" << insert_meta.status();

  LogSink::Instance().RemoveBackend(id);

  // Create the regular expression we want to match.
  std::regex re = [&bucket_name] {
    std::string regex = ".* POST .*";
    regex += "/b/" + bucket_name + "/o";
    regex += ".*userIp=.*";
    return std::regex(regex, std::regex_constants::egrep);
  }();

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [&re](std::string const& line) { return std::regex_match(line, re); });
  EXPECT_LT(0, count);

  auto status = client.DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertWithContentType) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), ContentType("text/plain"));
  ASSERT_TRUE(meta.ok()) << "status=" << meta.status();

  EXPECT_EQ("text/plain", meta->content_type());

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();
  EXPECT_EQ(object_name, insert->name());
  EXPECT_EQ(bucket_name, insert->bucket());

  // This operation should fail because the object already exists.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  EXPECT_FALSE(failure.ok()) << "metadata=" << *failure;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

TEST_F(ObjectInsertIntegrationTest, InsertXmlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_TRUE(client.ok()) << "status=" << client.status();

  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name, object_name, expected, Fields(""), IfGenerationMatch(0));
  ASSERT_TRUE(insert.ok()) << "status=" << insert.status();

  EXPECT_EQ(object_name, insert->name());
  EXPECT_EQ(bucket_name, insert->bucket());

  // This operation should fail because the object already exists.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name, object_name, expected, Fields(""), IfGenerationMatch(0));
  EXPECT_FALSE(failure.ok()) << "metadata=" << *failure;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_TRUE(status.ok()) << "status=" << status;
}

}  // anonymous namespace
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
              << " <project-id> <bucket-name>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const bucket_name = argv[2];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::ObjectTestEnvironment(project_id,
                                                        bucket_name));

  return RUN_ALL_TESTS();
}
