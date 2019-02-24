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
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

/// Store the project and instance captured from the command-line arguments.
class BucketTestEnvironment : public ::testing::Environment {
 public:
  BucketTestEnvironment(std::string project, std::string instance,
                        std::string topic) {
    project_id_ = std::move(project);
    bucket_name_ = std::move(instance);
    topic_ = std::move(topic);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& bucket_name() { return bucket_name_; }
  static std::string const& topic() { return topic_; }

 private:
  static std::string project_id_;
  static std::string bucket_name_;
  static std::string topic_;
};

std::string BucketTestEnvironment::project_id_;
std::string BucketTestEnvironment::bucket_name_;
std::string BucketTestEnvironment::topic_;

class BucketIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + BucketTestEnvironment::project_id();
  }
};

TEST_F(BucketIntegrationTest, BasicCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto buckets = client->ListBucketsForProject(project_id);
  std::vector<BucketMetadata> initial_buckets;
  for (auto&& b : buckets) {
    initial_buckets.emplace_back(std::move(b).value());
  }
  auto name_counter = [](std::string const& name,
                         std::vector<BucketMetadata> const& list) {
    return std::count_if(
        list.begin(), list.end(),
        [name](BucketMetadata const& m) { return m.name() == name; });
  };
  ASSERT_EQ(0, name_counter(bucket_name, initial_buckets))
      << "Test aborted. The bucket <" << bucket_name << "> already exists."
      << " This is unexpected as the test generates a random bucket name.";

  auto insert_meta =
      client->CreateBucketForProject(bucket_name, project_id, BucketMetadata());
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());

  buckets = client->ListBucketsForProject(project_id);
  std::vector<BucketMetadata> current_buckets;
  for (auto&& b : buckets) {
    current_buckets.emplace_back(std::move(b).value());
  }
  EXPECT_EQ(1U, name_counter(bucket_name, current_buckets));

  StatusOr<BucketMetadata> get_meta = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(get_meta);
  EXPECT_EQ(*insert_meta, *get_meta);

  // Create a request to update the metadata, change the storage class because
  // it is easy. And use either COLDLINE or NEARLINE depending on the existing
  // value.
  std::string desired_storage_class = storage_class::Coldline();
  if (get_meta->storage_class() == storage_class::Coldline()) {
    desired_storage_class = storage_class::Nearline();
  }
  BucketMetadata update = *get_meta;
  update.set_storage_class(desired_storage_class);
  StatusOr<BucketMetadata> updated_meta =
      client->UpdateBucket(bucket_name, update);
  ASSERT_STATUS_OK(updated_meta);
  EXPECT_EQ(desired_storage_class, updated_meta->storage_class());

  // Patch the metadata to change the storage class, add some lifecycle
  // rules, and the website settings.
  BucketMetadata desired_state = *updated_meta;
  LifecycleRule rule(LifecycleRule::ConditionConjunction(
                         LifecycleRule::MaxAge(30),
                         LifecycleRule::MatchesStorageClassStandard()),
                     LifecycleRule::Delete());
  desired_state.set_storage_class(storage_class::Standard())
      .set_lifecycle(BucketLifecycle{{rule}})
      .set_website(BucketWebsite{"index.html", "404.html"});

  StatusOr<BucketMetadata> patched =
      client->PatchBucket(bucket_name, *updated_meta, desired_state);
  ASSERT_STATUS_OK(patched);
  EXPECT_EQ(storage_class::Standard(), patched->storage_class());
  EXPECT_EQ(1U, patched->lifecycle().rule.size());

  // Patch the metadata again, this time remove billing and website settings.
  patched = client->PatchBucket(
      bucket_name, BucketMetadataPatchBuilder().ResetWebsite().ResetBilling());
  ASSERT_STATUS_OK(patched);
  EXPECT_FALSE(patched->has_billing());
  EXPECT_FALSE(patched->has_website());

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
  buckets = client->ListBucketsForProject(project_id);
  current_buckets.clear();
  for (auto&& b : buckets) {
    current_buckets.emplace_back(std::move(b).value());
  }
  EXPECT_EQ(0U, name_counter(bucket_name, current_buckets));
}

TEST_F(BucketIntegrationTest, FullPatch) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // We need to have an available bucket for logging ...
  std::string logging_name = MakeRandomBucketName();
  StatusOr<BucketMetadata> const logging_meta = client->CreateBucketForProject(
      logging_name, project_id, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("noAcl"));
  ASSERT_STATUS_OK(logging_meta);
  EXPECT_EQ(logging_name, logging_meta->name());

  // Create a Bucket, use the default settings for most fields, except the
  // storage class and location. Fetch the full attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client->CreateBucketForProject(
      bucket_name, project_id,
      BucketMetadata().set_location("US").set_storage_class(
          storage_class::MultiRegional()),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());

  // Patch every possible field in the metadata, to verify they work.
  BucketMetadata desired_state = *insert_meta;
  // acl()
  desired_state.mutable_acl().push_back(BucketAccessControl()
                                            .set_entity("allAuthenticatedUsers")
                                            .set_role("READER"));

  // billing()
  if (!desired_state.has_billing()) {
    desired_state.set_billing(BucketBilling(false));
  } else {
    desired_state.set_billing(
        BucketBilling(!desired_state.billing().requester_pays));
  }

  // cors()
  desired_state.mutable_cors().push_back(CorsEntry{86400, {"GET"}, {}, {}});

  // default_acl()
  desired_state.mutable_default_acl().push_back(
      ObjectAccessControl()
          .set_entity("allAuthenticatedUsers")
          .set_role("READER"));

  // encryption()
  // TODO(#1003) - need a valid KMS entry to set the encryption.

  // iam_configuration() - skipped, cannot set both ACL and iam_configuration in
  // the same bucket.

  // labels()
  desired_state.mutable_labels().emplace("test-label", "testing-full-patch");

  // lifecycle()
  LifecycleRule rule(LifecycleRule::ConditionConjunction(
                         LifecycleRule::MaxAge(30),
                         LifecycleRule::MatchesStorageClassStandard()),
                     LifecycleRule::Delete());
  desired_state.set_lifecycle(BucketLifecycle{{rule}});

  // logging()
  if (desired_state.has_logging()) {
    desired_state.reset_logging();
  } else {
    desired_state.set_logging(BucketLogging{logging_name, "test-log"});
  }

  // storage_class()
  desired_state.set_storage_class(storage_class::Coldline());

  // versioning()
  if (!desired_state.has_versioning()) {
    desired_state.enable_versioning();
  } else {
    desired_state.reset_versioning();
  }

  // website()
  if (desired_state.has_website()) {
    desired_state.reset_website();
  } else {
    desired_state.set_website(BucketWebsite{"index.html", "404.html"});
  }

  StatusOr<BucketMetadata> patched =
      client->PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);
  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_EQ(1U, std::count_if(patched->acl().begin(), patched->acl().end(),
                              [](BucketAccessControl const& x) {
                                return x.entity() == "allAuthenticatedUsers";
                              }));

  // billing()
  EXPECT_EQ(desired_state.billing_as_optional(),
            patched->billing_as_optional());

  // cors()
  EXPECT_EQ(desired_state.cors(), patched->cors());

  // default_acl() - cannot compare for equality because many fields are updated
  // with unknown values (entity_id, etag, etc)
  EXPECT_EQ(1U, std::count_if(patched->default_acl().begin(),
                              patched->default_acl().end(),
                              [](ObjectAccessControl const& x) {
                                return x.entity() == "allAuthenticatedUsers";
                              }));

  // encryption() - TODO(#1003) - verify the key was correctly used.

  // lifecycle()
  EXPECT_EQ(desired_state.lifecycle_as_optional(),
            patched->lifecycle_as_optional());

  // location()
  EXPECT_EQ(desired_state.location(), patched->location());

  // logging()
  EXPECT_EQ(desired_state.logging_as_optional(),
            patched->logging_as_optional());

  // storage_class()
  EXPECT_EQ(desired_state.storage_class(), patched->storage_class());

  // versioning()
  EXPECT_EQ(desired_state.versioning(), patched->versioning());

  // website()
  EXPECT_EQ(desired_state.website_as_optional(),
            patched->website_as_optional());

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
  status = client->DeleteBucket(logging_name);
  ASSERT_STATUS_OK(status);
}

// @test Verify that we can set the iam_configuration() in a Bucket.
TEST_F(BucketIntegrationTest, BucketPolicyOnlyPatch) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Create a Bucket, use the default settings for all fields. Fetch the full
  // attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client->CreateBucketForProject(
      bucket_name, project_id, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());

  // Patch the iam_configuration().
  BucketMetadata desired_state = *insert_meta;
  BucketIamConfiguration iam_configuration;
  iam_configuration.bucket_policy_only = BucketPolicyOnly{true};
  desired_state.set_iam_configuration(std::move(iam_configuration));

  StatusOr<BucketMetadata> patched =
      client->PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);

  ASSERT_TRUE(patched->has_iam_configuration()) << "patched=" << *patched;
  ASSERT_TRUE(patched->iam_configuration().bucket_policy_only)
      << "patched=" << *patched;

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, GetMetadata) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  EXPECT_EQ(bucket_name, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());
}

TEST_F(BucketIntegrationTest, GetMetadataFields) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name, Fields("name"));
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  EXPECT_TRUE(metadata->id().empty());
  EXPECT_TRUE(metadata->kind().empty());
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationMatch_Success) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  EXPECT_EQ(bucket_name, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());

  auto metadata2 = client->GetBucketMetadata(
      bucket_name, storage::Projection("noAcl"),
      storage::IfMetagenerationMatch(metadata->metageneration()));
  ASSERT_STATUS_OK(metadata2);
  EXPECT_EQ(*metadata2, *metadata);
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationNotMatch_Failure) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  EXPECT_EQ(bucket_name, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());

  auto metadata2 = client->GetBucketMetadata(
      bucket_name, storage::Projection("noAcl"),
      storage::IfMetagenerationNotMatch(metadata->metageneration()));
  EXPECT_FALSE(metadata2.ok()) << "metadata=" << *metadata2;
}

TEST_F(BucketIntegrationTest, AccessControlCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private" PredefinedAcl so
  // we know what the contents of the ACL will be.
  auto meta = client->CreateBucketForProject(
      bucket_name, project_id, BucketMetadata(), PredefinedAcl("private"),
      Projection("full"));
  ASSERT_STATUS_OK(meta);

  auto entity_name = MakeEntityName();

  auto name_counter = [](std::string const& name,
                         std::vector<BucketAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](BucketAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_FALSE(meta->acl().empty())
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_EQ(0, name_counter(entity_name, meta->acl()))
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
  EXPECT_EQ(1, name_counter(result->entity(), *current_acl));

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

  new_acl = *get_result;
  new_acl.set_role("OWNER");
  // Because this is a freshly created bucket, with a random name, we do not
  // worry about implementing optimistic concurrency control.
  get_result =
      client->PatchBucketAcl(bucket_name, entity_name, *get_result, new_acl);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(get_result->role(), new_acl.role());

  auto status = client->DeleteBucketAcl(bucket_name, entity_name);
  ASSERT_STATUS_OK(status);

  current_acl = client->ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_EQ(0, name_counter(result->entity(), *current_acl));

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, DefaultObjectAccessControlCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private"
  // PredefinedDefaultObjectAcl, that way we can predict the the contents of the
  // ACL.
  auto meta = client->CreateBucketForProject(
      bucket_name, project_id, BucketMetadata(),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));
  ASSERT_STATUS_OK(meta);

  auto entity_name = MakeEntityName();

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](ObjectAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_FALSE(meta->default_acl().empty())
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_EQ(0, name_counter(entity_name, meta->default_acl()))
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
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result->entity(), *current_acl));

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

  new_acl = *get_result;
  new_acl.set_role("OWNER");
  get_result =
      client->PatchDefaultObjectAcl(bucket_name, entity_name, *get_result,
                                    new_acl, IfMatchEtag(get_result->etag()));
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(get_result->role(), new_acl.role());

  auto status = client->DeleteDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_STATUS_OK(status);

  current_acl = client->ListDefaultObjectAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_EQ(0, name_counter(result->entity(), *current_acl));

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, NotificationsCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta =
      client->CreateBucketForProject(bucket_name, project_id, BucketMetadata());
  ASSERT_STATUS_OK(meta);

  auto current_notifications = client->ListNotifications(bucket_name);
  ASSERT_STATUS_OK(current_notifications);
  EXPECT_TRUE(current_notifications->empty())
      << "Test aborted. Non-empty notification list returned from newly"
      << " created bucket <" << bucket_name
      << ">. This is unexpected because the bucket name is chosen at random.";

  auto create = client->CreateNotification(
      bucket_name, BucketTestEnvironment::topic(), payload_format::JsonApiV1(),
      NotificationMetadata().append_event_type(event_type::ObjectFinalize()));
  ASSERT_STATUS_OK(create);

  EXPECT_EQ(payload_format::JsonApiV1(), create->payload_format());
  EXPECT_THAT(create->topic(), HasSubstr(BucketTestEnvironment::topic()));

  current_notifications = client->ListNotifications(bucket_name);
  ASSERT_STATUS_OK(current_notifications);
  auto count = std::count_if(current_notifications->begin(),
                             current_notifications->end(),
                             [create](NotificationMetadata const& x) {
                               return x.id() == create->id();
                             });
  EXPECT_EQ(1U, count) << "create=" << *create;

  auto get = client->GetNotification(bucket_name, create->id());
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(*create, *get);

  auto status = client->DeleteNotification(bucket_name, create->id());
  ASSERT_STATUS_OK(status);

  current_notifications = client->ListNotifications(bucket_name);
  ASSERT_STATUS_OK(current_notifications);
  count = std::count_if(current_notifications->begin(),
                        current_notifications->end(),
                        [create](NotificationMetadata const& x) {
                          return x.id() == create->id();
                        });
  EXPECT_EQ(0U, count) << "create=" << *create;

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, IamCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta =
      client->CreateBucketForProject(bucket_name, project_id, BucketMetadata());
  ASSERT_STATUS_OK(meta);

  StatusOr<IamPolicy> policy = client->GetBucketIamPolicy(bucket_name);
  ASSERT_STATUS_OK(policy);
  auto const& bindings = policy->bindings;
  // There must always be at least an OWNER for the Bucket.
  ASSERT_FALSE(bindings.end() ==
               bindings.find("roles/storage.legacyBucketOwner"));

  StatusOr<std::vector<BucketAccessControl>> acl =
      client->ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(acl);
  // Unfortunately we cannot compare the values in the ACL to the values in the
  // IamPolicy directly. The ids for entities have different formats, for
  // example: in ACL 'project-editors-123456789' and in IAM
  // 'projectEditors:my-project'. We can compare the counts though:
  std::set<std::string> expected_owners;
  for (auto const& entry : *acl) {
    if (entry.role() == "OWNER") {
      expected_owners.insert(entry.entity());
    }
  }
  std::set<std::string> actual_owners =
      bindings.at("roles/storage.legacyBucketOwner");
  EXPECT_EQ(expected_owners.size(), actual_owners.size());

  IamPolicy update = *policy;
  update.bindings.AddMember("roles/storage.objectViewer",
                            "allAuthenticatedUsers");

  StatusOr<IamPolicy> updated_policy =
      client->SetBucketIamPolicy(bucket_name, update);
  ASSERT_STATUS_OK(updated_policy);
  EXPECT_EQ(update.bindings, updated_policy->bindings);
  EXPECT_NE(update.etag, updated_policy->etag);

  std::vector<std::string> expected_permissions{
      "storage.objects.list", "storage.objects.get", "storage.objects.delete"};
  StatusOr<std::vector<std::string>> actual_permissions =
      client->TestBucketIamPermissions(bucket_name, expected_permissions);
  ASSERT_STATUS_OK(actual_permissions);
  EXPECT_THAT(*actual_permissions, ElementsAreArray(expected_permissions));

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, BucketLock) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta =
      client->CreateBucketForProject(bucket_name, project_id, BucketMetadata());
  ASSERT_STATUS_OK(meta);

  auto after_setting_retention_policy = client->PatchBucket(
      bucket_name,
      BucketMetadataPatchBuilder().SetRetentionPolicy(std::chrono::seconds(30)),
      IfMetagenerationMatch(meta->metageneration()));
  ASSERT_STATUS_OK(after_setting_retention_policy);

  auto after_locking = client->LockBucketRetentionPolicy(
      bucket_name, after_setting_retention_policy->metageneration());
  ASSERT_STATUS_OK(after_locking);

  ASSERT_TRUE(after_locking->has_retention_policy());
  ASSERT_TRUE(after_locking->retention_policy().is_locked);

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, BucketLockFailure) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // This should fail because the bucket does not exist.
  StatusOr<BucketMetadata> status =
      client->LockBucketRetentionPolicy(bucket_name, 42U);
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, ListFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Project IDs must end with a letter or number, test with an invalid ID.
  auto stream = client->ListBucketsForProject("Invalid-project-id-");
  auto it = stream.begin();
  StatusOr<BucketMetadata> metadata = *it;
  EXPECT_FALSE(metadata.ok()) << "value=" << metadata.value();
}

TEST_F(BucketIntegrationTest, CreateFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  // Try to create an invalid bucket (the name should not start with an
  // uppercase letter), the service (or testbench) will reject the request and
  // we should report that error correctly. For good measure, make the project
  // id invalid too.
  StatusOr<BucketMetadata> meta = client->CreateBucketForProject(
      "Invalid_Bucket_Name", "Invalid-project-id-", BucketMetadata());
  ASSERT_FALSE(meta.ok()) << "metadata=" << meta.value();
}

TEST_F(BucketIntegrationTest, GetFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to get information about a bucket that does not exist, or at least
  // it is very unlikely to exist, the name is random.
  auto status = client->GetBucketMetadata(bucket_name);
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, DeleteFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to delete a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client->DeleteBucket(bucket_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, UpdateFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to update a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client->UpdateBucket(bucket_name, BucketMetadata());
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, PatchFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to update a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client->PatchBucket(bucket_name, BucketMetadataPatchBuilder());
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, GetBucketIamPolicyFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to get information about a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto policy = client->GetBucketIamPolicy(bucket_name);
  EXPECT_FALSE(policy.ok()) << "value=" << policy.value();
}

TEST_F(BucketIntegrationTest, SetBucketIamPolicyFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to set the IAM policy on a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto policy = client->SetBucketIamPolicy(bucket_name, IamPolicy{});
  EXPECT_FALSE(policy.ok()) << "value=" << policy.value();
}

TEST_F(BucketIntegrationTest, TestBucketIamPermissionsFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to set the IAM policy on a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto items = client->TestBucketIamPermissions(bucket_name, {});
  EXPECT_FALSE(items.ok()) << "items[0]=" << items.value().front();
}

TEST_F(BucketIntegrationTest, ListAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // This operation should fail because the target bucket does not exist.
  auto list = client->ListBucketAcl(bucket_name);
  EXPECT_FALSE(list.ok()) << "list[0]=" << list.value().front();
}

TEST_F(BucketIntegrationTest, CreateAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->CreateBucketAcl(bucket_name, entity_name, "READER");
  EXPECT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, GetAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->GetBucketAcl(bucket_name, entity_name);
  EXPECT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, UpdateAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->UpdateBucketAcl(
      bucket_name,
      BucketAccessControl().set_entity(entity_name).set_role("READER"));
  EXPECT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, PatchAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->PatchBucketAcl(
      bucket_name, entity_name, BucketAccessControl(),
      BucketAccessControl().set_entity(entity_name).set_role("READER"));
  EXPECT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, DeleteAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->DeleteBucketAcl(bucket_name, entity_name);
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, ListDefaultAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->ListDefaultObjectAcl(bucket_name).status();
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, CreateDefaultAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client->CreateDefaultObjectAcl(bucket_name, entity_name, "READER")
          .status();
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, GetDefaultAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->GetDefaultObjectAcl(bucket_name, entity_name).status();
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, UpdateDefaultAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client
          ->UpdateDefaultObjectAcl(
              bucket_name,
              ObjectAccessControl().set_entity(entity_name).set_role("READER"))
          .status();
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, PatchDefaultAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client
          ->PatchDefaultObjectAcl(
              bucket_name, entity_name, ObjectAccessControl(),
              ObjectAccessControl().set_entity(entity_name).set_role("READER"))
          .status();
  EXPECT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, DeleteDefaultAccessControlFailure) {
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->DeleteDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_FALSE(status.ok());
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <bucket> <topic>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const bucket_name = argv[2];
  std::string const topic = argv[3];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::BucketTestEnvironment(project_id, bucket_name,
                                                        topic));

  return RUN_ALL_TESTS();
}
