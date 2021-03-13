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
#include "google/cloud/storage/testing/remove_stale_buckets.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
// TODO(#5929) - remove once deprecated functions are removed
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::AclEntityNames;
using ::google::cloud::testing_util::ContainsOnce;
using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Not;

class BucketIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
    topic_name_ = google::cloud::internal::GetEnv(
                      "GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME")
                      .value_or("");
    ASSERT_FALSE(topic_name_.empty());
    service_account_ = google::cloud::internal::GetEnv(
                           "GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT")
                           .value_or("");
    ASSERT_FALSE(service_account_.empty());
  }

  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + project_id_;
  }

  std::string project_id_;
  std::string bucket_name_;
  std::string topic_name_;
  std::string service_account_;
};

TEST_F(BucketIntegrationTest, BasicCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // We use this test to remove any buckets created by the integration tests
  // more than 48 hours ago.
  testing::RemoveStaleBuckets(
      *client, RandomBucketNamePrefix(),
      std::chrono::system_clock::now() - std::chrono::hours(48));

  auto list_bucket_names = [&client, this] {
    std::vector<std::string> names;
    for (auto b : client->ListBucketsForProject(project_id_)) {
      EXPECT_STATUS_OK(b);
      if (!b) break;
      names.push_back(b->name());
    }
    return names;
  };
  ASSERT_THAT(list_bucket_names(), Not(Contains(bucket_name)))
      << "Test aborted. The bucket <" << bucket_name << "> already exists."
      << " This is unexpected as the test generates a random bucket name.";

  auto insert_meta = client->CreateBucketForProject(bucket_name, project_id_,
                                                    BucketMetadata());
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());
  EXPECT_THAT(list_bucket_names(), ContainsOnce(bucket_name));

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
  EXPECT_EQ(1, patched->lifecycle().rule.size());

  // Patch the metadata again, this time remove billing and website settings.
  patched = client->PatchBucket(
      bucket_name, BucketMetadataPatchBuilder().ResetWebsite().ResetBilling());
  ASSERT_STATUS_OK(patched);
  EXPECT_FALSE(patched->has_billing());
  EXPECT_FALSE(patched->has_website());

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(list_bucket_names(), Not(Contains(bucket_name)));
}

TEST_F(BucketIntegrationTest, CreatePredefinedAcl) {
  std::vector<PredefinedAcl> test_values{
      PredefinedAcl::AuthenticatedRead(), PredefinedAcl::Private(),
      PredefinedAcl::ProjectPrivate(),    PredefinedAcl::PublicRead(),
      PredefinedAcl::PublicReadWrite(),
  };

  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  for (auto const& acl : test_values) {
    SCOPED_TRACE(std::string("Testing with ") +
                 acl.well_known_parameter_name() + "=" + acl.value());
    std::string bucket_name = MakeRandomBucketName();

    auto metadata = client->CreateBucketForProject(
        bucket_name, project_id_, BucketMetadata(), PredefinedAcl(acl));
    ASSERT_STATUS_OK(metadata);
    EXPECT_EQ(bucket_name, metadata->name());

    // Wait at least 2 seconds before trying to create / delete another bucket.
    if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));

    auto status = client->DeleteBucket(bucket_name);
    ASSERT_STATUS_OK(status);

    // Wait at least 2 seconds before trying to create / delete another bucket.
    if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

TEST_F(BucketIntegrationTest, CreatePredefinedDefaultObjectAcl) {
  std::vector<PredefinedDefaultObjectAcl> test_values{
      PredefinedDefaultObjectAcl::AuthenticatedRead(),
      PredefinedDefaultObjectAcl::BucketOwnerFullControl(),
      PredefinedDefaultObjectAcl::BucketOwnerRead(),
      PredefinedDefaultObjectAcl::Private(),
      PredefinedDefaultObjectAcl::ProjectPrivate(),
      PredefinedDefaultObjectAcl::PublicRead(),
  };

  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  for (auto const& acl : test_values) {
    SCOPED_TRACE(std::string("Testing with ") +
                 acl.well_known_parameter_name() + "=" + acl.value());
    std::string bucket_name = MakeRandomBucketName();

    auto metadata = client->CreateBucketForProject(
        bucket_name, project_id_, BucketMetadata(),
        PredefinedDefaultObjectAcl(acl));
    ASSERT_STATUS_OK(metadata);
    EXPECT_EQ(bucket_name, metadata->name());

    // Wait at least 2 seconds before trying to create / delete another bucket.
    if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));

    auto status = client->DeleteBucket(bucket_name);
    ASSERT_STATUS_OK(status);

    // Wait at least 2 seconds before trying to create / delete another bucket.
    if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

TEST_F(BucketIntegrationTest, PatchLifecycleConditions) {
  std::vector<LifecycleRuleCondition> test_values{
      LifecycleRule::MaxAge(30),
      LifecycleRule::CreatedBefore(absl::CivilDay(2020, 7, 26)),
      LifecycleRule::IsLive(false),
      LifecycleRule::MatchesStorageClassArchive(),
      LifecycleRule::MatchesStorageClasses(
          {storage_class::Standard(), storage_class::Nearline()}),
      LifecycleRule::MatchesStorageClassStandard(),
      // Skip this one because it requires creating a regional bucket (the
      // default is multi-regional US), and that felt like too much of a hassle
      //   LifecycleRule::MatchesStorageClassRegional(),
      LifecycleRule::MatchesStorageClassMultiRegional(),
      LifecycleRule::MatchesStorageClassNearline(),
      LifecycleRule::MatchesStorageClassColdline(),
      LifecycleRule::MatchesStorageClassDurableReducedAvailability(),
  };

  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  auto original = client->CreateBucketForProject(bucket_name, project_id_,
                                                 BucketMetadata{});
  ASSERT_STATUS_OK(original);
  EXPECT_EQ(bucket_name, original->name());

  for (auto const& condition : test_values) {
    std::ostringstream os;
    os << "testing with " << condition;
    auto const str = std::move(os).str();
    SCOPED_TRACE(str);

    auto updated = client->PatchBucket(
        bucket_name, BucketMetadataPatchBuilder{}.SetLifecycle({{LifecycleRule{
                         condition, LifecycleRule::Delete()}}}));
    // We do not use an ASSERT_STATUS_OK() here because we want to continue and
    // delete the temporary bucket.
    EXPECT_STATUS_OK(updated);
    if (updated) {
      EXPECT_TRUE(updated->has_lifecycle()) << "updated = " << *updated;
    }
  }

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, FullPatch) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // We need to have an available bucket for logging ...
  std::string logging_name = MakeRandomBucketName();
  StatusOr<BucketMetadata> const logging_meta = client->CreateBucketForProject(
      logging_name, project_id_, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("noAcl"));
  ASSERT_STATUS_OK(logging_meta);
  EXPECT_EQ(logging_name, logging_meta->name());

  // Wait at least 2 seconds before trying to create / delete another bucket.
  if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  // Create a Bucket, use the default settings for most fields, except the
  // storage class and location. Fetch the full attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client->CreateBucketForProject(
      bucket_name, project_id_,
      BucketMetadata().set_location("US").set_storage_class(
          storage_class::Standard()),
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
  EXPECT_THAT(AclEntityNames(patched->acl()),
              ContainsOnce("allAuthenticatedUsers"));

  // billing()
  EXPECT_EQ(desired_state.billing_as_optional(),
            patched->billing_as_optional());

  // cors()
  EXPECT_EQ(desired_state.cors(), patched->cors());

  // default_acl() - cannot compare for equality because many fields are updated
  // with unknown values (entity_id, etag, etc)
  EXPECT_THAT(AclEntityNames(patched->default_acl()),
              ContainsOnce("allAuthenticatedUsers"));

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
  // Wait at least 2 seconds before trying to create / delete another bucket.
  if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  status = client->DeleteBucket(logging_name);
  ASSERT_STATUS_OK(status);
}

// @test Verify that we can set the iam_configuration() in a Bucket.
TEST_F(BucketIntegrationTest, UniformBucketLevelAccessPatch) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a Bucket, use the default settings for all fields. Fetch the full
  // attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client->CreateBucketForProject(
      bucket_name, project_id_, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());

  // Patch the iam_configuration().
  BucketMetadata desired_state = *insert_meta;
  BucketIamConfiguration iam_configuration;
  iam_configuration.uniform_bucket_level_access =
      UniformBucketLevelAccess{true, {}};
  desired_state.set_iam_configuration(std::move(iam_configuration));

  StatusOr<BucketMetadata> patched =
      client->PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);

  ASSERT_TRUE(patched->has_iam_configuration()) << "patched=" << *patched;
  ASSERT_TRUE(patched->iam_configuration().uniform_bucket_level_access)
      << "patched=" << *patched;

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, GetMetadata) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name_);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name_, metadata->name());
  EXPECT_EQ(bucket_name_, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());
}

TEST_F(BucketIntegrationTest, GetMetadataFields) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name_, Fields("name"));
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name_, metadata->name());
  EXPECT_TRUE(metadata->id().empty());
  EXPECT_TRUE(metadata->kind().empty());
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name_);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name_, metadata->name());
  EXPECT_EQ(bucket_name_, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());

  auto metadata2 = client->GetBucketMetadata(
      bucket_name_, storage::Projection("noAcl"),
      storage::IfMetagenerationMatch(metadata->metageneration()));
  ASSERT_STATUS_OK(metadata2);
  EXPECT_EQ(*metadata2, *metadata);
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationNotMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto metadata = client->GetBucketMetadata(bucket_name_);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name_, metadata->name());
  EXPECT_EQ(bucket_name_, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());

  auto metadata2 = client->GetBucketMetadata(
      bucket_name_, storage::Projection("noAcl"),
      storage::IfMetagenerationNotMatch(metadata->metageneration()));
  EXPECT_THAT(metadata2, Not(IsOk())) << "metadata=" << metadata2.value();
}

TEST_F(BucketIntegrationTest, AccessControlCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private" PredefinedAcl so
  // we know what the contents of the ACL will be.
  auto meta = client->CreateBucketForProject(
      bucket_name, project_id_, BucketMetadata(), PredefinedAcl("private"),
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
  EXPECT_THAT(AclEntityNames(*current_acl), Not(Contains(result->entity())));

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, DefaultObjectAccessControlCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private"
  // PredefinedDefaultObjectAcl, that way we can predict the the contents of the
  // ACL.
  auto meta = client->CreateBucketForProject(
      bucket_name, project_id_, BucketMetadata(),
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
  EXPECT_THAT(AclEntityNames(meta->default_acl()), Not(Contains(entity_name)));

  status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, NotificationsCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta = client->CreateBucketForProject(bucket_name, project_id_,
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
      bucket_name, topic_name_, payload_format::JsonApiV1(),
      NotificationMetadata().append_event_type(event_type::ObjectFinalize()));
  ASSERT_STATUS_OK(create);

  EXPECT_EQ(payload_format::JsonApiV1(), create->payload_format());
  EXPECT_THAT(create->topic(), HasSubstr(topic_name_));
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

TEST_F(BucketIntegrationTest, IamCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta = client->CreateBucketForProject(bucket_name, project_id_,
                                             BucketMetadata());
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

TEST_F(BucketIntegrationTest, NativeIamCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta = client->CreateBucketForProject(bucket_name, project_id_,
                                             BucketMetadata());
  ASSERT_STATUS_OK(meta);

  StatusOr<NativeIamPolicy> policy =
      client->GetNativeBucketIamPolicy(bucket_name);
  ASSERT_STATUS_OK(policy);
  auto const& bindings = policy->bindings();
  // There must always be at least an OWNER for the Bucket.
  ASSERT_TRUE(google::cloud::internal::ContainsIf(
      bindings, [](NativeIamBinding const& binding) {
        return binding.role() == "roles/storage.legacyBucketOwner";
      }));

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
  std::set<std::string> actual_owners = std::accumulate(
      bindings.begin(), bindings.end(), std::set<std::string>(),
      [](std::set<std::string> acc, NativeIamBinding const& binding) {
        if (binding.role() == "roles/storage.legacyBucketOwner") {
          acc.insert(binding.members().begin(), binding.members().end());
        }
        return acc;
      });
  EXPECT_EQ(expected_owners.size(), actual_owners.size());

  NativeIamPolicy update = *policy;
  bool role_updated = false;
  for (auto& binding : update.bindings()) {
    if (binding.role() != "roles/storage.objectViewer") {
      continue;
    }
    role_updated = true;
    auto& members = binding.members();
    if (!google::cloud::internal::Contains(members, "allAuthenticatedUsers")) {
      members.emplace_back("allAuthenticatedUsers");
    }
  }
  if (!role_updated) {
    update.bindings().emplace_back(NativeIamBinding(
        "roles/storage.objectViewer", {"allAuthenticatedUsers"}));
  }

  StatusOr<NativeIamPolicy> updated_policy =
      client->SetNativeBucketIamPolicy(bucket_name, update);
  ASSERT_STATUS_OK(updated_policy);

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
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  auto meta = client->CreateBucketForProject(bucket_name, project_id_,
                                             BucketMetadata());
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
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // This should fail because the bucket does not exist.
  StatusOr<BucketMetadata> status =
      client->LockBucketRetentionPolicy(bucket_name, 42U);
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, ListFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Project IDs must end with a letter or number, test with an invalid ID.
  auto stream = client->ListBucketsForProject("Invalid-project-id-");
  auto it = stream.begin();
  StatusOr<BucketMetadata> metadata = *it;
  EXPECT_THAT(metadata, Not(IsOk())) << "value=" << metadata.value();
}

TEST_F(BucketIntegrationTest, CreateFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Try to create an invalid bucket (the name should not start with an
  // uppercase letter), the service (or emulator) will reject the request and
  // we should report that error correctly. For good measure, make the project
  // id invalid too.
  StatusOr<BucketMetadata> meta = client->CreateBucketForProject(
      "Invalid_Bucket_Name", "Invalid-project-id-", BucketMetadata());
  ASSERT_FALSE(meta.ok()) << "metadata=" << meta.value();
}

TEST_F(BucketIntegrationTest, GetFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to get information about a bucket that does not exist, or at least
  // it is very unlikely to exist, the name is random.
  auto status = client->GetBucketMetadata(bucket_name);
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, DeleteFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to delete a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client->DeleteBucket(bucket_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, UpdateFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to update a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client->UpdateBucket(bucket_name, BucketMetadata());
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, PatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to update a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client->PatchBucket(bucket_name, BucketMetadataPatchBuilder());
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, GetBucketIamPolicyFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to get information about a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto policy = client->GetBucketIamPolicy(bucket_name);
  EXPECT_THAT(policy, Not(IsOk())) << "value=" << policy.value();
}

TEST_F(BucketIntegrationTest, SetBucketIamPolicyFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to set the IAM policy on a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto policy = client->SetBucketIamPolicy(bucket_name, IamPolicy{});
  EXPECT_THAT(policy, Not(IsOk())) << "value=" << policy.value();
}

TEST_F(BucketIntegrationTest, TestBucketIamPermissionsFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // Try to set the IAM policy on a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto items = client->TestBucketIamPermissions(bucket_name, {});
  EXPECT_THAT(items, Not(IsOk())) << "items[0]=" << items.value().front();
}

TEST_F(BucketIntegrationTest, ListAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // This operation should fail because the target bucket does not exist.
  auto list = client->ListBucketAcl(bucket_name);
  EXPECT_THAT(list, Not(IsOk())) << "list[0]=" << list.value().front();
}

TEST_F(BucketIntegrationTest, CreateAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->CreateBucketAcl(bucket_name, entity_name, "READER");
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, GetAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->GetBucketAcl(bucket_name, entity_name);
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, UpdateAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->UpdateBucketAcl(
      bucket_name,
      BucketAccessControl().set_entity(entity_name).set_role("READER"));
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, PatchAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client->PatchBucketAcl(
      bucket_name, entity_name, BucketAccessControl(),
      BucketAccessControl().set_entity(entity_name).set_role("READER"));
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketIntegrationTest, DeleteAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->DeleteBucketAcl(bucket_name, entity_name);
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, ListDefaultAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->ListDefaultObjectAcl(bucket_name).status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, CreateDefaultAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client->CreateDefaultObjectAcl(bucket_name, entity_name, "READER")
          .status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, GetDefaultAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->GetDefaultObjectAcl(bucket_name, entity_name).status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, UpdateDefaultAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
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
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, PatchDefaultAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
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
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, DeleteDefaultAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client->DeleteDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, NativeIamWithRequestedPolicyVersion) {
  std::string bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test.
  BucketMetadata original = BucketMetadata();
  BucketIamConfiguration configuration;
  configuration.uniform_bucket_level_access =
      UniformBucketLevelAccess{true, {}};

  original.set_iam_configuration(std::move(configuration));

  auto meta =
      client->CreateBucketForProject(bucket_name, project_id_, original);
  ASSERT_STATUS_OK(meta) << bucket_name;

  StatusOr<NativeIamPolicy> policy =
      client->GetNativeBucketIamPolicy(bucket_name, RequestedPolicyVersion(1));

  ASSERT_STATUS_OK(policy);
  ASSERT_EQ(1, policy->version());

  auto const& bindings = policy->bindings();
  // There must always be at least an OWNER for the Bucket.
  ASSERT_TRUE(google::cloud::internal::ContainsIf(
      bindings, [](NativeIamBinding const& binding) {
        return binding.role() == "roles/storage.legacyBucketOwner";
      }));

  NativeIamPolicy update = *policy;
  bool role_updated = false;
  for (auto& binding : update.bindings()) {
    if (binding.role() != "roles/storage.objectViewer") {
      continue;
    }
    role_updated = true;

    auto& members = binding.members();
    if (!google::cloud::internal::Contains(members, "allAuthenticatedUsers")) {
      members.emplace_back("serviceAccount:" + service_account_);
    }
  }
  if (!role_updated) {
    update.bindings().emplace_back(NativeIamBinding(
        "roles/storage.objectViewer", {"serviceAccount:" + service_account_},
        NativeExpression(
            "request.time < timestamp(\"2019-07-01T00:00:00.000Z\")",
            "Expires_July_1_2019", "Expires on July 1, 2019")));
    update.set_version(3);
  }

  StatusOr<NativeIamPolicy> updated_policy =
      client->SetNativeBucketIamPolicy(bucket_name, update);
  ASSERT_STATUS_OK(updated_policy);

  StatusOr<NativeIamPolicy> policy_with_condition =
      client->GetNativeBucketIamPolicy(bucket_name, RequestedPolicyVersion(3));
  ASSERT_STATUS_OK(policy_with_condition);
  ASSERT_EQ(3, policy_with_condition->version());

  std::vector<std::string> expected_permissions{
      "storage.objects.list", "storage.objects.get", "storage.objects.delete"};
  StatusOr<std::vector<std::string>> actual_permissions =
      client->TestBucketIamPermissions(bucket_name, expected_permissions);
  ASSERT_STATUS_OK(actual_permissions);
  EXPECT_THAT(*actual_permissions, ElementsAreArray(expected_permissions));

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}
}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
