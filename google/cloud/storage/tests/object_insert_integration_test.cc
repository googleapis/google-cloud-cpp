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
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::CountMatchingEntities;
using ::google::cloud::testing_util::IsOk;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Not;

constexpr auto kJsonEnvVar = "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON";
constexpr auto kP12EnvVar = "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12";

class ObjectInsertIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  ObjectInsertIntegrationTest()
      : application_credentials_("GOOGLE_APPLICATION_CREDENTIALS", {}) {}

  void SetUp() override {
    if (!UsingEmulator()) {
      // This test was chosen (more or less arbitrarily) to validate that both
      // P12 and JSON credentials are usable in production. The positives for
      // this test are (1) it is relatively short (less than 60 seconds), (2) it
      // actually performs multiple operations against production.
      std::string const env = GetParam();
      if (UsingGrpc() && env == kP12EnvVar) {
        // TODO(#5116): gRPC doesn't support PKCS #12 keys.
        GTEST_SKIP() << "Skipping because gRPC doesn't support PKCS #12 keys";
      }
      auto value = google::cloud::internal::GetEnv(env.c_str()).value_or("");
      // The p12 test is only run on certain platforms, so if it's not set, skip
      // the test rather than failing.
      if (value.empty() && env == kP12EnvVar) {
        GTEST_SKIP() << "Skipping because ${" << env << "} is not set";
      }
      ASSERT_FALSE(value.empty())
          << "Expected non-empty value for ${" << env << "}";
      google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", value);
    }
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  ::google::cloud::testing_util::ScopedEnvironment application_credentials_;
  std::string bucket_name_;
};

TEST_P(ObjectInsertIntegrationTest, SimpleInsertWithNonUrlSafeName) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      DisableCrc32cChecksum(true), DisableMD5Hash(true));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertWithNonUrlSafeName) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, MultipartInsertWithNonUrlSafeName) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, InsertWithMD5) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      MD5HashValue("96HF9K981B+JfoQuTVnyCg=="));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, InsertWithComputedMD5) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      MD5HashValue(ComputeMD5Hash(expected)));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertWithMD5) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0), Fields(""),
      MD5HashValue("96HF9K981B+JfoQuTVnyCg=="));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, InsertWithMetadata) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata()
                             .upsert_metadata("test-key", "test-value")
                             .set_content_type("text/plain")));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  EXPECT_TRUE(meta->has_metadata("test-key"));
  EXPECT_EQ("test-value", meta->metadata("test-key"));
  EXPECT_EQ("text/plain", meta->content_type());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclAuthenticatedRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::AuthenticatedRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclBucketOwnerFullControl) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name_, Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerFullControl(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclBucketOwnerRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name_, Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclPrivate) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::Private(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclProjectPrivate) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::ProjectPrivate(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclPublicRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::PublicRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclAuthenticatedRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::AuthenticatedRead(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name_, object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest,
       XmlInsertPredefinedAclBucketOwnerFullControl) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name_, Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerFullControl(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name_, object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclBucketOwnerRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name_, Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerRead(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name_, object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclPrivate) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::Private(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name_, object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclProjectPrivate) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::ProjectPrivate(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name_, object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclPublicRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::PublicRead(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta =
      client->GetObjectMetadata(bucket_name_, object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;
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
TEST_P(ObjectInsertIntegrationTest, InsertWithQuotaUser) {
  Client client(Options{}.set<TracingComponentsOption>({"raw-client", "http"}));
  auto object_name = MakeRandomObjectName();

  testing_util::ScopedLog log;
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), QuotaUser("test-quota-user"));
  ASSERT_STATUS_OK(insert_meta);
  ScheduleForDelete(*insert_meta);

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr(" POST "),
                             HasSubstr("/b/" + bucket_name_ + "/o"),
                             HasSubstr("quotaUser=test-quota-user"))));
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
TEST_P(ObjectInsertIntegrationTest, InsertWithUserIp) {
  Client client(Options{}.set<TracingComponentsOption>({"raw-client", "http"}));
  auto object_name = MakeRandomObjectName();

  testing_util::ScopedLog log;
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), UserIp("127.0.0.1"));
  ASSERT_STATUS_OK(insert_meta);
  ScheduleForDelete(*insert_meta);

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr(" POST "),
                             HasSubstr("/b/" + bucket_name_ + "/o"),
                             HasSubstr("userIp=127.0.0.1"))));
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
TEST_P(ObjectInsertIntegrationTest, InsertWithUserIpBlank) {
  Client client(Options{}.set<TracingComponentsOption>({"raw-client", "http"}));
  auto object_name = MakeRandomObjectName();

  // Make sure at least one connection was created before we run the test, the
  // IP address can only be obtained once the first request to a given endpoint
  // is completed.
  {
    auto seed_object_name = MakeRandomObjectName();
    auto insert =
        client.InsertObject(bucket_name_, seed_object_name, LoremIpsum());
    ASSERT_STATUS_OK(insert);
    ScheduleForDelete(*insert);
  }

  testing_util::ScopedLog log;
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), UserIp(""));
  ASSERT_STATUS_OK(insert_meta);
  ScheduleForDelete(*insert_meta);

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr(" POST "),
                             HasSubstr("/b/" + bucket_name_ + "/o"),
                             HasSubstr("userIp="))));
}

TEST_P(ObjectInsertIntegrationTest, InsertWithContentType) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), ContentType("text/plain"));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ("text/plain", meta->content_type());
}

TEST_P(ObjectInsertIntegrationTest, InsertFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(object_name, insert->name());
  EXPECT_EQ(bucket_name_, insert->bucket());

  // This operation should fail because the object already exists.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  EXPECT_THAT(failure, Not(IsOk())) << "metadata=" << failure.value();
}

TEST_P(ObjectInsertIntegrationTest, InsertXmlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert = client->InsertObject(
      bucket_name_, object_name, expected, Fields(""), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  EXPECT_EQ(object_name, insert->name());
  EXPECT_EQ(bucket_name_, insert->bucket());

  // This operation should fail because the object already exists.
  StatusOr<ObjectMetadata> failure = client->InsertObject(
      bucket_name_, object_name, expected, Fields(""), IfGenerationMatch(0));
  EXPECT_THAT(failure, Not(IsOk())) << "metadata=" << failure.value();
}

INSTANTIATE_TEST_SUITE_P(ObjectInsertWithJsonCredentialsTest,
                         ObjectInsertIntegrationTest,
                         ::testing::Values(kJsonEnvVar));
INSTANTIATE_TEST_SUITE_P(ObjectInsertWithP12CredentialsTest,
                         ObjectInsertIntegrationTest,
                         ::testing::Values(kP12EnvVar));

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
