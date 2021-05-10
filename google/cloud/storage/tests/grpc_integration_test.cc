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
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <crc32c/crc32c.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <vector>

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/grpc_error_delegate.h"
#include <grpcpp/grpcpp.h>
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::testing::AclEntityNames;
using ::google::cloud::testing_util::ContainsOnce;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  GrpcIntegrationTest()
      : grpc_config_("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", {}) {}

  void SetUp() override {
    std::string const grpc_config_value = GetParam();
    google::cloud::internal::SetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                    grpc_config_value);
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty()) << "GOOGLE_CLOUD_PROJECT is not set";

    topic_name_ = google::cloud::internal::GetEnv(
                      "GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME")
                      .value_or("");
    ASSERT_FALSE(topic_name_.empty())
        << "GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME is not set";
  }
  std::string project_id() const { return project_id_; }
  std::string topic_name() const { return topic_name_; }

  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + project_id_;
  }

 private:
  std::string project_id_;
  std::string topic_name_;
  testing_util::ScopedEnvironment grpc_config_;
};

TEST_P(GrpcIntegrationTest, BucketCRUD) {
  auto client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto bucket_metadata = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  EXPECT_EQ(bucket_name, bucket_metadata->name());

  auto list_bucket_names = [&client, this] {
    std::vector<std::string> names;
    for (auto b : client->ListBucketsForProject(project_id())) {
      EXPECT_STATUS_OK(b);
      if (!b) break;
      names.push_back(b->name());
    }
    return names;
  };
  EXPECT_THAT(list_bucket_names(), Contains(bucket_name));

  auto get = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(bucket_name, get->id());
  EXPECT_EQ(bucket_name, get->name());

  // Create a request to update the metadata, change the storage class because
  // it is easy. And use either COLDLINE or NEARLINE depending on the existing
  // value.
  std::string desired_storage_class = storage_class::Coldline();
  if (get->storage_class() == storage_class::Coldline()) {
    desired_storage_class = storage_class::Nearline();
  }
  BucketMetadata update = *get;
  update.set_storage_class(desired_storage_class);
  StatusOr<BucketMetadata> updated_meta =
      client->UpdateBucket(bucket_name, update);
  ASSERT_STATUS_OK(updated_meta);
  EXPECT_EQ(desired_storage_class, updated_meta->storage_class());

  auto delete_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_status);
  EXPECT_THAT(list_bucket_names(), Not(Contains(bucket_name)));
}

TEST_P(GrpcIntegrationTest, BucketAccessControlCRUD) {
  // TODO(#5673): Enable this.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private" PredefinedAcl so
  // we know what the contents of the ACL will be.
  std::string bucket_name = MakeRandomBucketName();
  auto meta = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata(), PredefinedAcl("private"),
      Projection("full"));
  ASSERT_STATUS_OK(meta);

  auto entity_name = MakeEntityName();

  ASSERT_FALSE(meta->acl().empty())
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_THAT(AclEntityNames(meta->acl()), Not(Contains(entity_name)))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << entity_name
      << "> in its ACL.  This is unexpected because the bucket was just"
      << " created with a predefine ACL which should preclude this result.";

  StatusOr<BucketAccessControl> result =
      client->CreateBucketAcl(bucket_name, entity_name, "OWNER");
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("OWNER", result->role());

  StatusOr<std::vector<BucketAccessControl>> current_acl =
      client->ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_FALSE(current_acl->empty());
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_THAT(AclEntityNames(*current_acl), ContainsOnce(result->entity()));

  StatusOr<BucketAccessControl> get_result =
      client->GetBucketAcl(bucket_name, entity_name);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(*get_result, *result);

  BucketAccessControl new_acl = *get_result;
  new_acl.set_role("READER");
  auto updated_result = client->UpdateBucketAcl(bucket_name, new_acl);
  ASSERT_STATUS_OK(updated_result);
  EXPECT_EQ("READER", updated_result->role());

  get_result = client->GetBucketAcl(bucket_name, entity_name);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(*get_result, *updated_result);

  auto status = client->DeleteBucketAcl(bucket_name, entity_name);
  ASSERT_STATUS_OK(status);

  current_acl = client->ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl), Not(Contains(result->entity())));

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(GrpcIntegrationTest, DefaultObjectAccessControlCRUD) {
  // TODO(#5673): Enable this.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private"
  // PredefinedDefaultObjectAcl, that way we can predict the the contents of the
  // ACL.
  std::string bucket_name = MakeRandomBucketName();
  auto meta = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata(),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));
  ASSERT_STATUS_OK(meta);

  auto entity_name = MakeEntityName();

  ASSERT_FALSE(meta->default_acl().empty())
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_THAT(AclEntityNames(meta->default_acl()), Not(Contains(entity_name)))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << entity_name
      << "> in its ACL.  This is unexpected because the bucket was just"
      << " created with a predefine ACL which should preclude this result.";

  StatusOr<ObjectAccessControl> result =
      client->CreateDefaultObjectAcl(bucket_name, entity_name, "OWNER");
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("OWNER", result->role());

  auto current_acl = client->ListDefaultObjectAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_FALSE(current_acl->empty());
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id_>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_THAT(AclEntityNames(meta->default_acl()),
              ContainsOnce(result->entity()));

  auto get_result = client->GetDefaultObjectAcl(bucket_name, entity_name);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(*get_result, *result);

  ObjectAccessControl new_acl = *get_result;
  new_acl.set_role("READER");
  auto updated_result = client->UpdateDefaultObjectAcl(bucket_name, new_acl);
  ASSERT_STATUS_OK(updated_result);

  EXPECT_EQ(updated_result->role(), "READER");
  get_result = client->GetDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_EQ(*get_result, *updated_result);

  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(get_result->role(), new_acl.role());

  auto status = client->DeleteDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_STATUS_OK(status);

  current_acl = client->ListDefaultObjectAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(meta->default_acl()), Not(Contains(entity_name)));

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(GrpcIntegrationTest, NotificationsCRUD) {
  // TODO(#5673): Enable this.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  std::string bucket_name = MakeRandomBucketName();
  auto meta = client->CreateBucketForProject(bucket_name, project_id(),
                                             BucketMetadata());
  ASSERT_STATUS_OK(meta);

  auto notification_ids = [&client, bucket_name] {
    std::vector<std::string> ids;
    auto list = client->ListNotifications(bucket_name);
    EXPECT_STATUS_OK(list);
    for (auto const& notification : *list) ids.push_back(notification.id());
    return ids;
  };
  ASSERT_TRUE(notification_ids().empty())
      << "Test aborted. Non-empty notification list returned from newly"
      << " created bucket <" << bucket_name
      << ">. This is unexpected because the bucket name is chosen at random.";

  auto create = client->CreateNotification(
      bucket_name, topic_name(), payload_format::JsonApiV1(),
      NotificationMetadata().append_event_type(event_type::ObjectFinalize()));
  ASSERT_STATUS_OK(create);

  EXPECT_EQ(payload_format::JsonApiV1(), create->payload_format());
  EXPECT_THAT(create->topic(), HasSubstr(topic_name()));
  EXPECT_THAT(notification_ids(), ContainsOnce(create->id()))
      << "create=" << *create;

  auto get = client->GetNotification(bucket_name, create->id());
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(*create, *get);

  auto status = client->DeleteNotification(bucket_name, create->id());
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(notification_ids(), Not(Contains(create->id())))
      << "create=" << *create;

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(GrpcIntegrationTest, ObjectCRUD) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  EXPECT_EQ(bucket_name, bucket_metadata->name());

  auto object_metadata = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(object_metadata);

  auto stream = client->ReadObject(bucket_name, object_name);

  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(LoremIpsum(), actual);
  EXPECT_STATUS_OK(stream.status());

  auto delete_object_status = client->DeleteObject(
      bucket_name, object_name, Generation(object_metadata->generation()));
  EXPECT_STATUS_OK(delete_object_status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_P(GrpcIntegrationTest, NativeIamCRUD) {
  // TODO(#5673): Enable this.
  if (!UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  std::string bucket_name = MakeRandomBucketName();
  auto meta = client->CreateBucketForProject(bucket_name, project_id(),
                                             BucketMetadata());
  ASSERT_STATUS_OK(meta);

  auto set = client->SetNativeBucketIamPolicy(
      bucket_name, NativeIamPolicy({NativeIamBinding(
                                       "test-role", {"user@test.example.com"})},
                                   "test-etag", 3));
  ASSERT_STATUS_OK(set);

  EXPECT_TRUE(google::cloud::internal::ContainsIf(
      set->bindings(), [](NativeIamBinding const& binding) {
        return binding.role() == "test-role";
      }));
  EXPECT_TRUE(google::cloud::internal::ContainsIf(
      set->bindings(), [](NativeIamBinding const& binding) {
        return google::cloud::internal::Contains(binding.members(),
                                                 "user@test.example.com");
      }));
  EXPECT_EQ(set->version(), 3);

  auto get = client->GetNativeBucketIamPolicy(bucket_name);
  ASSERT_STATUS_OK(get);
  EXPECT_TRUE(google::cloud::internal::ContainsIf(
      get->bindings(), [](NativeIamBinding const& binding) {
        return binding.role() == "test-role";
      }));
  EXPECT_TRUE(google::cloud::internal::ContainsIf(
      get->bindings(), [](NativeIamBinding const& binding) {
        return google::cloud::internal::Contains(binding.members(),
                                                 "user@test.example.com");
      }));
  EXPECT_EQ(get->version(), set->version());
  EXPECT_EQ(get->etag(), set->etag());

  auto test = client->TestBucketIamPermissions(
      bucket_name, {"storage.buckets.get", "storage.buckets.setIamPolicy",
                    "storage.objects.update"});
  ASSERT_STATUS_OK(test);
  EXPECT_THAT(*test, UnorderedElementsAre("storage.buckets.get",
                                          "storage.buckets.setIamPolicy",
                                          "storage.objects.update"));

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(GrpcIntegrationTest, WriteResume) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  std::string session_id;
  {
    auto old_os =
        client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                            NewResumableUploadSession());
    ASSERT_TRUE(old_os.good()) << "status=" << old_os.metadata().status();
    session_id = old_os.resumable_session_id();
    std::move(old_os).Suspend();
  }

  auto os = client->WriteObject(bucket_name, object_name,
                                RestoreResumableUploadSession(session_id));
  ASSERT_TRUE(os.good()) << "status=" << os.metadata().status();
  EXPECT_EQ(session_id, os.resumable_session_id());
  os << LoremIpsum();
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  if (UsingEmulator()) {
    EXPECT_TRUE(meta.has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_P(GrpcIntegrationTest, InsertLarge) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  // Insert an object that is larger than 4 MiB, and its size is not a
  // multiple of 256 KiB.
  auto const desired_size = 8 * 1024 * 1024L + 253 * 1024 + 15;
  auto data = MakeRandomData(desired_size);
  auto metadata = client->InsertObject(bucket_name, object_name, data,
                                       IfGenerationMatch(0));
  EXPECT_STATUS_OK(metadata);
  if (!metadata) {
    auto delete_bucket_status = client->DeleteBucket(bucket_name);
    EXPECT_STATUS_OK(delete_bucket_status);
    return;
  }
  EXPECT_EQ(desired_size, metadata->size());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_P(GrpcIntegrationTest, StreamLargeChunks) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  // Insert an object in chunks larger than 4 MiB each.
  auto const desired_size = 8 * 1024 * 1024L;
  auto data = MakeRandomData(desired_size);
  auto stream =
      client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.good());
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.good());
  stream.Close();
  EXPECT_FALSE(stream.bad());
  EXPECT_STATUS_OK(stream.metadata());

  EXPECT_EQ(2 * desired_size, stream.metadata()->size());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

INSTANTIATE_TEST_SUITE_P(GrpcIntegrationMediaTest, GrpcIntegrationTest,
                         ::testing::Values("media"));
INSTANTIATE_TEST_SUITE_P(GrpcIntegrationMetadataTest, GrpcIntegrationTest,
                         ::testing::Values("metadata"));

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
