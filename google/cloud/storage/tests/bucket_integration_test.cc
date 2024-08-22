// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::AclEntityNames;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::IsSubsetOf;
using ::testing::Not;
using ::testing::Property;
using ::testing::UnorderedElementsAreArray;

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
  auto client = MakeBucketIntegrationTestClient();

  // We use this test to remove any buckets created by the integration tests
  // more than 48 hours ago.
  testing::RemoveStaleBuckets(
      client, RandomBucketNamePrefix(),
      std::chrono::system_clock::now() - std::chrono::hours(48));

  auto list_bucket_names = [&client, this] {
    std::vector<std::string> names;
    for (auto b : client.ListBucketsForProject(project_id_)) {
      EXPECT_STATUS_OK(b);
      if (!b) break;
      names.push_back(b->name());
    }
    return names;
  };
  ASSERT_THAT(list_bucket_names(), Not(Contains(bucket_name)))
      << "Test aborted. The bucket <" << bucket_name << "> already exists."
      << " This is unexpected as the test generates a random bucket name.";

  // Always request a full projection as this works with REST and gRPC
  auto insert_meta = client.CreateBucketForProject(
      bucket_name, project_id_, BucketMetadata(), Projection::Full());
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());
  EXPECT_THAT(list_bucket_names(), Contains(bucket_name).Times(1));

  StatusOr<BucketMetadata> get_meta =
      client.GetBucketMetadata(bucket_name, Projection::Full());
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
      client.UpdateBucket(bucket_name, update);
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
      client.PatchBucket(bucket_name, *updated_meta, desired_state);
  ASSERT_STATUS_OK(patched);
  EXPECT_EQ(storage_class::Standard(), patched->storage_class());
  EXPECT_EQ(1, patched->lifecycle().rule.size());

  // Patch the metadata again, this time remove billing and website settings.
  // The emulator does not support this feature for gRPC.
  if (!UsingEmulator() || !UsingGrpc()) {
    patched = client.PatchBucket(
        bucket_name,
        BucketMetadataPatchBuilder().ResetWebsite().ResetBilling());
    ASSERT_STATUS_OK(patched);
    // It does not matter if the `billing` compound is set. Only that it has the
    // same effect as-if it was not set, i.e., it has the default value.
    EXPECT_EQ(patched->billing_as_optional().value_or(BucketBilling{false}),
              BucketBilling{false});
    EXPECT_FALSE(patched->has_website());
  }

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(list_bucket_names(), Not(Contains(bucket_name)));
}

TEST_F(BucketIntegrationTest, CreateDuplicate) {
  auto client = MakeBucketIntegrationTestClient();
  auto const bucket_name = MakeRandomBucketName();
  auto metadata =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata());
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  // Wait at least 2 seconds before trying to create / delete another bucket.
  if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));

  auto dup =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata());
  EXPECT_THAT(dup, StatusIs(StatusCode::kAlreadyExists));
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

  auto client = MakeBucketIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  auto original =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata{});
  ASSERT_STATUS_OK(original);
  EXPECT_EQ(bucket_name, original->name());

  for (auto const& condition : test_values) {
    std::ostringstream os;
    os << "testing with " << condition;
    auto const str = std::move(os).str();
    SCOPED_TRACE(str);

    auto updated = client.PatchBucket(
        bucket_name, BucketMetadataPatchBuilder{}.SetLifecycle({{LifecycleRule{
                         condition, LifecycleRule::Delete()}}}));
    // We do not use an ASSERT_STATUS_OK() here because we want to continue and
    // delete the temporary bucket.
    EXPECT_STATUS_OK(updated);
    if (updated) {
      EXPECT_TRUE(updated->has_lifecycle()) << "updated = " << *updated;
    }
  }

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, FullPatch) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeBucketIntegrationTestClient();

  // We need to have an available bucket for logging ...
  std::string logging_name = MakeRandomBucketName();
  StatusOr<BucketMetadata> const logging_meta = client.CreateBucketForProject(
      logging_name, project_id_, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("noAcl"));
  ASSERT_STATUS_OK(logging_meta);
  EXPECT_EQ(logging_name, logging_meta->name());

  // Wait at least 2 seconds before trying to create / delete another bucket.
  if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  // Create a Bucket, use the default settings for most fields, except the
  // storage class and location. Fetch the full attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client.CreateBucketForProject(
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
  // NOLINTNEXTLINE(modernize-use-emplace) - brace initialization
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
  if (desired_state.has_versioning() && desired_state.versioning()->enabled) {
    desired_state.reset_versioning();
  } else {
    desired_state.enable_versioning();
  }

  // website()
  if (desired_state.has_website()) {
    desired_state.reset_website();
  } else {
    desired_state.set_website(BucketWebsite{"index.html", "404.html"});
  }

  StatusOr<BucketMetadata> patched =
      client.PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);
  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_THAT(AclEntityNames(patched->acl()),
              Contains("allAuthenticatedUsers").Times(1));

  // billing()
  EXPECT_EQ(desired_state.billing_as_optional(),
            patched->billing_as_optional());

  // cors()
  EXPECT_EQ(desired_state.cors(), patched->cors());

  // default_acl() - cannot compare for equality because many fields are updated
  // with unknown values (entity_id, etag, etc)
  EXPECT_THAT(AclEntityNames(patched->default_acl()),
              Contains("allAuthenticatedUsers").Times(1));

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

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
  // Wait at least 2 seconds before trying to create / delete another bucket.
  if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  status = client.DeleteBucket(logging_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that we can set the iam_configuration() in a Bucket.
TEST_F(BucketIntegrationTest, UniformBucketLevelAccessPatch) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient();

  // Create a Bucket, use the default settings for all fields. Fetch the full
  // attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client.CreateBucketForProject(
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
      client.PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);

  ASSERT_TRUE(patched->has_iam_configuration()) << "patched=" << *patched;
  ASSERT_TRUE(patched->iam_configuration().uniform_bucket_level_access)
      << "patched=" << *patched;

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that we can set the iam_configuration() in a Bucket.
TEST_F(BucketIntegrationTest, PublicAccessPreventionPatch) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient();

  // Create a Bucket, use the default settings for all fields. Fetch the full
  // attributes of the bucket.
  StatusOr<BucketMetadata> const insert_meta = client.CreateBucketForProject(
      bucket_name, project_id_, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());

  // Patch the iam_configuration().
  BucketMetadata desired_state = *insert_meta;
  BucketIamConfiguration iam_configuration;
  iam_configuration.public_access_prevention = PublicAccessPreventionEnforced();
  desired_state.set_iam_configuration(std::move(iam_configuration));

  StatusOr<BucketMetadata> patched =
      client.PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);

  ASSERT_TRUE(patched->has_iam_configuration()) << "patched=" << *patched;
  ASSERT_TRUE(patched->iam_configuration().public_access_prevention)
      << "patched=" << *patched;

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that we can set the RPO in a Bucket.
TEST_F(BucketIntegrationTest, RpoPatch) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient();

  auto insert_meta = client.CreateBucketForProject(
      bucket_name, project_id_,
      BucketMetadata().set_rpo(RpoAsyncTurbo()).set_location("NAM4"),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  ScheduleForDelete(*insert_meta);
  EXPECT_EQ(bucket_name, insert_meta->name());
  EXPECT_EQ("ASYNC_TURBO", insert_meta->rpo());

  // Patch the rpo().
  BucketMetadata desired_state = *insert_meta;
  desired_state.set_rpo(RpoDefault());

  StatusOr<BucketMetadata> patched =
      client.PatchBucket(bucket_name, *insert_meta, desired_state);
  ASSERT_STATUS_OK(patched);

  EXPECT_EQ(patched->rpo(), RpoDefault()) << "patched=" << *patched;

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that we can use MatchesPrefixes() and MatchesSuffixes()
TEST_F(BucketIntegrationTest, MatchesPrefixSuffixPatch) {
  auto const bucket_name = MakeRandomBucketName();
  auto client = MakeBucketIntegrationTestClient();

  auto const insert_meta = client.CreateBucketForProject(
      bucket_name, project_id_, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  ScheduleForDelete(*insert_meta);

  // Patch the lifecycle().
  auto lifecycle =
      insert_meta->lifecycle_as_optional().value_or(BucketLifecycle{});
  lifecycle.rule.emplace_back(
      LifecycleRule::ConditionConjunction(
          LifecycleRule::MaxAge(30),
          LifecycleRule::MatchesPrefixes({"p1/", "p2/"}),
          LifecycleRule::MatchesSuffixes({".test", ".bad"})),
      LifecycleRule::Delete());

  auto patched = client.PatchBucket(
      bucket_name, BucketMetadataPatchBuilder().SetLifecycle(lifecycle));
  ASSERT_STATUS_OK(patched);

  ASSERT_TRUE(patched->has_lifecycle()) << "patched=" << *patched;
  ASSERT_EQ(patched->lifecycle(), lifecycle);

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, GetMetadata) {
  auto client = MakeIntegrationTestClient();

  auto metadata = client.GetBucketMetadata(bucket_name_);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name_, metadata->name());
  EXPECT_EQ(bucket_name_, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());
}

TEST_F(BucketIntegrationTest, GetMetadataFields) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();
  auto client = MakeIntegrationTestClient();

  auto metadata = client.GetBucketMetadata(bucket_name_, Fields("name"));
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name_, metadata->name());
  // This field is normally returned by JSON and gRPC. In this case it should be
  // empty, because we only requested the `name` field.
  EXPECT_THAT(metadata->storage_class(), IsEmpty());
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationMatchSuccess) {
  auto client = MakeBucketIntegrationTestClient();

  std::string bucket_name = MakeRandomBucketName();
  auto create =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata{});
  ASSERT_STATUS_OK(create) << bucket_name;

  auto metadata = client.GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  EXPECT_EQ(bucket_name, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());

  auto metadata2 = client.GetBucketMetadata(
      bucket_name, storage::Projection("noAcl"),
      storage::IfMetagenerationMatch(metadata->metageneration()));
  ASSERT_STATUS_OK(metadata2);
  EXPECT_EQ(*metadata2, *metadata);

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationNotMatchFailure) {
  auto client = MakeBucketIntegrationTestClient();

  std::string bucket_name = MakeRandomBucketName();
  auto create =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata{});
  ASSERT_STATUS_OK(create) << bucket_name;

  auto metadata = client.GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ(bucket_name, metadata->name());
  EXPECT_EQ(bucket_name, metadata->id());
  EXPECT_EQ("storage#bucket", metadata->kind());

  auto metadata2 = client.GetBucketMetadata(
      bucket_name, storage::Projection("noAcl"),
      storage::IfMetagenerationNotMatch(metadata->metageneration()));
  EXPECT_THAT(metadata2, Not(IsOk())) << "metadata=" << metadata2.value();

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, NativeIamCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeBucketIntegrationTestClient();

  // Create a new bucket to run the test.
  auto meta =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata());
  ASSERT_STATUS_OK(meta);

  StatusOr<NativeIamPolicy> policy =
      client.GetNativeBucketIamPolicy(bucket_name);
  ASSERT_STATUS_OK(policy);
  auto const& bindings = policy->bindings();
  // There must always be at least an OWNER for the Bucket.
  ASSERT_THAT(bindings, Contains(Property(&NativeIamBinding::role,
                                          "roles/storage.legacyBucketOwner")));

  auto acl = client.ListBucketAcl(bucket_name);
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
    if (binding.role() != "roles/storage.objectViewer") continue;
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
      client.SetNativeBucketIamPolicy(bucket_name, update);
  ASSERT_STATUS_OK(updated_policy);

  std::vector<std::string> expected_permissions{
      "storage.objects.list", "storage.objects.get", "storage.objects.delete"};
  auto const actual_permissions =
      client.TestBucketIamPermissions(bucket_name, expected_permissions);
  ASSERT_STATUS_OK(actual_permissions);
  EXPECT_THAT(*actual_permissions, Not(IsEmpty()));
  // In most runs, you would find that `actual_permissions` is equal to
  // `expected_permissions`. But testing for this is inherently flaky. It can
  // take up to 7 minutes for IAM changes to propagate through the systems.
  //     https://cloud.google.com/iam/docs/faq#access_revoke
  EXPECT_THAT(*actual_permissions, IsSubsetOf(expected_permissions));
  if (UsingEmulator()) {
    EXPECT_THAT(*actual_permissions, expected_permissions);
  }

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, BucketLock) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient();

  // Create a new bucket to run the test.
  auto meta =
      client.CreateBucketForProject(bucket_name, project_id_, BucketMetadata());
  ASSERT_STATUS_OK(meta);

  auto after_setting_retention_policy = client.PatchBucket(
      bucket_name,
      BucketMetadataPatchBuilder().SetRetentionPolicy(std::chrono::seconds(30)),
      IfMetagenerationMatch(meta->metageneration()));
  ASSERT_STATUS_OK(after_setting_retention_policy);

  auto after_locking = client.LockBucketRetentionPolicy(
      bucket_name, after_setting_retention_policy->metageneration());
  ASSERT_STATUS_OK(after_locking);

  ASSERT_TRUE(after_locking->has_retention_policy());
  ASSERT_TRUE(after_locking->retention_policy().is_locked);

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketIntegrationTest, BucketLockFailure) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient();

  // This should fail because the bucket does not exist.
  StatusOr<BucketMetadata> status =
      client.LockBucketRetentionPolicy(bucket_name, 42U);
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(BucketIntegrationTest, ListFailure) {
  auto client = MakeIntegrationTestClient();

  // Project IDs must end with a letter or number, test with an invalid ID.
  auto stream = client.ListBucketsForProject("Invalid-project-id-");
  auto it = stream.begin();
  StatusOr<BucketMetadata> metadata = *it;
  EXPECT_THAT(metadata, Not(IsOk())) << "value=" << metadata.value();
}

TEST_F(BucketIntegrationTest, CreateFailure) {
  auto client = MakeBucketIntegrationTestClient();

  // Try to create an invalid bucket (the name should not start with an
  // uppercase letter), the service (or emulator) will reject the request and
  // we should report that error correctly. For good measure, make the project
  // id invalid too.
  StatusOr<BucketMetadata> meta = client.CreateBucketForProject(
      "Invalid_Bucket_Name", "Invalid-project-id-", BucketMetadata());
  ASSERT_FALSE(meta.ok()) << "metadata=" << meta.value();
}

TEST_F(BucketIntegrationTest, GetFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to get information about a bucket that does not exist, or at least
  // it is very unlikely to exist, the name is random.
  auto status = client.GetBucketMetadata(bucket_name);
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, DeleteFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to delete a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client.DeleteBucket(bucket_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(BucketIntegrationTest, UpdateFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to update a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client.UpdateBucket(bucket_name, BucketMetadata());
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, PatchFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to update a bucket that does not exist, or at least it is very unlikely
  // to exist, the name is random.
  auto status = client.PatchBucket(bucket_name, BucketMetadataPatchBuilder());
  ASSERT_FALSE(status.ok()) << "value=" << status.value();
}

TEST_F(BucketIntegrationTest, GetNativeBucketIamPolicyFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to get information about a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto policy = client.GetNativeBucketIamPolicy(bucket_name);
  EXPECT_THAT(policy, Not(IsOk())) << "value=" << policy.value();
}

TEST_F(BucketIntegrationTest, SetNativeBucketIamPolicyFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to set the IAM policy on a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto policy =
      client.SetNativeBucketIamPolicy(bucket_name, NativeIamPolicy{{}, ""});
  EXPECT_THAT(policy, Not(IsOk())) << "value=" << policy.value();
}

TEST_F(BucketIntegrationTest, TestBucketIamPermissionsFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // Try to set the IAM policy on a bucket that does not exist, or at least it
  // is very unlikely to exist, the name is random.
  auto items = client.TestBucketIamPermissions(bucket_name, {});
  EXPECT_THAT(items, Not(IsOk())) << "items[0]=" << items.value().front();
}

TEST_F(BucketIntegrationTest, NativeIamWithRequestedPolicyVersion) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();

  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeBucketIntegrationTestClient();

  // Create a new bucket to run the test.
  BucketMetadata original = BucketMetadata();
  BucketIamConfiguration configuration;
  configuration.uniform_bucket_level_access =
      UniformBucketLevelAccess{true, {}};

  original.set_iam_configuration(std::move(configuration));

  auto meta = client.CreateBucketForProject(bucket_name, project_id_, original);
  ASSERT_STATUS_OK(meta) << bucket_name;

  StatusOr<NativeIamPolicy> policy =
      client.GetNativeBucketIamPolicy(bucket_name, RequestedPolicyVersion(1));

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
      client.SetNativeBucketIamPolicy(bucket_name, update);
  ASSERT_STATUS_OK(updated_policy);

  StatusOr<NativeIamPolicy> policy_with_condition =
      client.GetNativeBucketIamPolicy(bucket_name, RequestedPolicyVersion(3));
  ASSERT_STATUS_OK(policy_with_condition);
  ASSERT_EQ(3, policy_with_condition->version());

  std::vector<std::string> expected_permissions{
      "storage.objects.list", "storage.objects.get", "storage.objects.delete"};
  StatusOr<std::vector<std::string>> actual_permissions =
      client.TestBucketIamPermissions(bucket_name, expected_permissions);
  ASSERT_STATUS_OK(actual_permissions);
  EXPECT_THAT(*actual_permissions,
              UnorderedElementsAreArray(expected_permissions));

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
