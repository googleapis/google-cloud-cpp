// Copyright 2019 Google LLC
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
#include "google/cloud/log.h"
#include "google/cloud/testing_util/expect_exception.h"
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
using ::testing::Not;

class ObjectRewriteIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  std::string bucket_name_;
};

TEST_F(ObjectRewriteIntegrationTest, Copy) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto source_object_name = MakeRandomObjectName();
  auto destination_object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name_, source_object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);
  ScheduleForDelete(*source_meta);

  EXPECT_EQ(source_object_name, source_meta->name());
  EXPECT_EQ(bucket_name_, source_meta->bucket());

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, source_object_name, bucket_name_, destination_object_name,
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_EQ(destination_object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  EXPECT_EQ("text/plain", meta->content_type());

  auto stream = client->ReadObject(bucket_name_, destination_object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectRewriteIntegrationTest, CopyPredefinedAclAuthenticatedRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> original = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, object_name, bucket_name_, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::AuthenticatedRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;
}

TEST_F(ObjectRewriteIntegrationTest, CopyPredefinedAclBucketOwnerFullControl) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name_, Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> original = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, object_name, bucket_name_, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::BucketOwnerFullControl(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;
}

TEST_F(ObjectRewriteIntegrationTest, CopyPredefinedAclBucketOwnerRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client->GetBucketMetadata(bucket_name_, Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> original = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, object_name, bucket_name_, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::BucketOwnerRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;
}

TEST_F(ObjectRewriteIntegrationTest, CopyPredefinedAclPrivate) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> original = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, object_name, bucket_name_, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::Private(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;
}

TEST_F(ObjectRewriteIntegrationTest, CopyPredefinedAclProjectPrivate) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> original = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, object_name, bucket_name_, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::ProjectPrivate(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;
}

TEST_F(ObjectRewriteIntegrationTest, CopyPredefinedAclPublicRead) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> original = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

  StatusOr<ObjectMetadata> meta = client->CopyObject(
      bucket_name_, object_name, bucket_name_, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::PublicRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);
  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;
}

TEST_F(ObjectRewriteIntegrationTest, ComposeSimple) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  // Compose new of object using previously created object
  auto composed_object_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> source_objects = {{object_name, {}, {}},
                                                     {object_name, {}, {}}};
  StatusOr<ObjectMetadata> composed_meta = client->ComposeObject(
      bucket_name_, source_objects, composed_object_name,
      WithObjectMetadata(ObjectMetadata().set_content_type("plain/text")));
  ASSERT_STATUS_OK(composed_meta);
  ScheduleForDelete(*composed_meta);
  EXPECT_EQ(meta->size() * 2, composed_meta->size());
}

TEST_F(ObjectRewriteIntegrationTest, ComposedUsingEncryptedObject) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string content = LoremIpsum();
  EncryptionKeyData key = MakeEncryptionKeyData();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name_, object_name, content,
                           IfGenerationMatch(0), EncryptionKey(key));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  ASSERT_TRUE(meta->has_customer_encryption());
  EXPECT_EQ("AES256", meta->customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta->customer_encryption().key_sha256);

  // Compose new of object using previously created object
  auto composed_object_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> source_objects = {{object_name, {}, {}},
                                                     {object_name, {}, {}}};
  StatusOr<ObjectMetadata> composed_meta = client->ComposeObject(
      bucket_name_, source_objects, composed_object_name, EncryptionKey(key));
  ASSERT_STATUS_OK(composed_meta);
  ScheduleForDelete(*composed_meta);

  EXPECT_EQ(meta->size() * 2, composed_meta->size());
}

TEST_F(ObjectRewriteIntegrationTest, RewriteSimple) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto source_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name_, source_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);
  ScheduleForDelete(*source_meta);

  // Rewrite object into a new object.
  auto object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> rewritten_meta = client->RewriteObjectBlocking(
      bucket_name_, source_name, bucket_name_, object_name);
  ASSERT_STATUS_OK(rewritten_meta);
  ScheduleForDelete(*rewritten_meta);

  EXPECT_EQ(bucket_name_, rewritten_meta->bucket());
  EXPECT_EQ(object_name, rewritten_meta->name());
}

TEST_F(ObjectRewriteIntegrationTest, RewriteEncrypted) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto source_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  EncryptionKeyData source_key = MakeEncryptionKeyData();
  StatusOr<ObjectMetadata> source_meta =
      client->InsertObject(bucket_name_, source_name, LoremIpsum(),
                           IfGenerationMatch(0), EncryptionKey(source_key));
  ASSERT_STATUS_OK(source_meta);
  ScheduleForDelete(*source_meta);

  // Compose new of object using previously created object
  auto object_name = MakeRandomObjectName();
  EncryptionKeyData dest_key = MakeEncryptionKeyData();
  ObjectRewriter rewriter = client->RewriteObject(
      bucket_name_, source_name, bucket_name_, object_name,
      SourceEncryptionKey(source_key), EncryptionKey(dest_key));

  StatusOr<ObjectMetadata> rewritten_meta = rewriter.Result();
  ASSERT_STATUS_OK(rewritten_meta);
  ScheduleForDelete(*rewritten_meta);

  EXPECT_EQ(bucket_name_, rewritten_meta->bucket());
  EXPECT_EQ(object_name, rewritten_meta->name());
}

TEST_F(ObjectRewriteIntegrationTest, RewriteLarge) {
  // The emulator always requires multiple iterations to copy this object.
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto source_name = MakeRandomObjectName();

  std::string large_text;
  std::size_t const lines = 8 * 1024 * 1024 / 128;
  for (std::size_t i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, 127,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "0123456789");
    large_text += line + "\n";
  }

  StatusOr<ObjectMetadata> source_meta = client->InsertObject(
      bucket_name_, source_name, large_text, IfGenerationMatch(0));
  ASSERT_STATUS_OK(source_meta);
  ScheduleForDelete(*source_meta);

  // Rewrite object into a new object.
  auto object_name = MakeRandomObjectName();
  ObjectRewriter writer = client->RewriteObject(bucket_name_, source_name,
                                                bucket_name_, object_name);

  StatusOr<ObjectMetadata> rewritten_meta =
      writer.ResultWithProgressCallback([](StatusOr<RewriteProgress> const& p) {
        ASSERT_STATUS_OK(p);
        EXPECT_TRUE((p->total_bytes_rewritten < p->object_size) xor p->done)
            << "p.done=" << p->done << ", p.object_size=" << p->object_size
            << ", p.total_bytes_rewritten=" << p->total_bytes_rewritten;
      });
  ASSERT_STATUS_OK(rewritten_meta);
  ScheduleForDelete(*rewritten_meta);

  EXPECT_EQ(bucket_name_, rewritten_meta->bucket());
  EXPECT_EQ(object_name, rewritten_meta->name());
}

TEST_F(ObjectRewriteIntegrationTest, CopyFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto source_object_name = MakeRandomObjectName();
  auto destination_object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto meta = client->CopyObject(bucket_name_, source_object_name, bucket_name_,
                                 destination_object_name);
  EXPECT_THAT(meta, Not(IsOk())) << "value=" << meta.value();
}

TEST_F(ObjectRewriteIntegrationTest, ComposeFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto composed_object_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> source_objects = {{object_name, {}, {}},
                                                     {object_name, {}, {}}};

  // This operation should fail because the source object does not exist.
  auto meta =
      client->ComposeObject(bucket_name_, source_objects, composed_object_name);
  EXPECT_THAT(meta, Not(IsOk())) << "value=" << meta.value();
}

TEST_F(ObjectRewriteIntegrationTest, RewriteFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto source_object_name = MakeRandomObjectName();
  auto destination_object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  StatusOr<ObjectMetadata> metadata = client->RewriteObjectBlocking(
      bucket_name_, source_object_name, bucket_name_, destination_object_name);
  EXPECT_THAT(metadata, Not(IsOk()));
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
