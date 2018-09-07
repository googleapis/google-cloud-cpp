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
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
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

class BucketIntegrationTest : public ::testing::Test {
 protected:
  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + BucketTestEnvironment::project_id();
  }

  std::string MakeRandomBucketName() {
    // The total length of this bucket name must be <= 63 characters,
    static std::string const prefix = "gcs-cpp-test-bucket-";
    static std::size_t const kMaxBucketNameLength = 63;
    std::size_t const max_random_characters =
        kMaxBucketNameLength - prefix.size();
    return prefix + google::cloud::internal::Sample(
                        generator_, static_cast<int>(max_random_characters),
                        "abcdefghijklmnopqrstuvwxyz012456789");
  }

  std::string MakeRandomObjectName() {
    static std::string const prefix = "bucket-integration-test-";
    return prefix + google::cloud::internal::Sample(generator_, 64,
                                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                    "abcdefghijklmnopqrstuvwxyz"
                                                    "012456789");
  }

  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

TEST_F(BucketIntegrationTest, BasicCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  Client client;

  auto buckets = client.ListBucketsForProject(project_id);
  std::vector<BucketMetadata> initial_buckets(buckets.begin(), buckets.end());
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
      client.CreateBucketForProject(bucket_name, project_id, BucketMetadata());
  EXPECT_EQ(bucket_name, insert_meta.name());

  buckets = client.ListBucketsForProject(project_id);
  std::vector<BucketMetadata> current_buckets(buckets.begin(), buckets.end());
  EXPECT_EQ(1U, name_counter(bucket_name, current_buckets));

  BucketMetadata get_meta = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(insert_meta, get_meta);

  // Create a request to update the metadata, change the storage class because
  // it is easy. And use either COLDLINE or NEARLINE depending on the existing
  // value.
  std::string desired_storage_class = storage_class::Coldline();
  if (get_meta.storage_class() == storage_class::Coldline()) {
    desired_storage_class = storage_class::Nearline();
  }
  BucketMetadata update = get_meta;
  update.set_storage_class(desired_storage_class);
  BucketMetadata updated_meta = client.UpdateBucket(bucket_name, update);
  EXPECT_EQ(desired_storage_class, updated_meta.storage_class());

  // Patch the metadata to change the storage class, add some lifecycle
  // rules, and the website settings.
  BucketMetadata desired_state = updated_meta;
  LifecycleRule rule(LifecycleRule::ConditionConjunction(
                         LifecycleRule::MaxAge(30),
                         LifecycleRule::MatchesStorageClassStandard()),
                     LifecycleRule::Delete());
  desired_state.set_storage_class(storage_class::Standard())
      .set_lifecycle(BucketLifecycle{{rule}})
      .set_website(BucketWebsite{"index.html", "404.html"});

  BucketMetadata patched =
      client.PatchBucket(bucket_name, updated_meta, desired_state);
  EXPECT_EQ(storage_class::Standard(), patched.storage_class());
  EXPECT_EQ(1U, patched.lifecycle().rule.size());

  // Patch the metadata again, this time remove billing and website settings.
  patched = client.PatchBucket(
      bucket_name, BucketMetadataPatchBuilder().ResetWebsite().ResetBilling());
  EXPECT_FALSE(patched.has_billing());
  EXPECT_FALSE(patched.has_website());

  client.DeleteBucket(bucket_name);
  buckets = client.ListBucketsForProject(project_id);
  current_buckets.assign(buckets.begin(), buckets.end());
  EXPECT_EQ(0U, name_counter(bucket_name, current_buckets));
}

TEST_F(BucketIntegrationTest, FullPatch) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  Client client;

  // We need to have an available bucket for logging ...
  std::string logging_name = MakeRandomBucketName();
  BucketMetadata const logging_meta = client.CreateBucketForProject(
      logging_name, project_id, BucketMetadata(), PredefinedAcl("private"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("noAcl"));
  EXPECT_EQ(logging_name, logging_meta.name());

  // Create a Bucket, use the default settings for most fields, except the
  // storage class and location. Fetch the full attributes of the bucket.
  BucketMetadata const insert_meta = client.CreateBucketForProject(
      bucket_name, project_id,
      BucketMetadata().set_location("US").set_storage_class(
          storage_class::MultiRegional()),
      PredefinedAcl("private"), PredefinedDefaultObjectAcl("projectPrivate"),
      Projection("full"));
  EXPECT_EQ(bucket_name, insert_meta.name());

  // Patch every possible field in the metadata, to verify they work.
  BucketMetadata desired_state = insert_meta;
  // acl()
  desired_state.mutable_acl().push_back(BucketAccessControl()
                                            .set_entity("allAuthenticatedUsers")
                                            .set_role("READER"));

  // billing()
  if (not desired_state.has_billing()) {
    desired_state.set_billing(BucketBilling(false));
  } else {
    desired_state.set_billing(
        BucketBilling(not desired_state.billing().requester_pays));
  }

  // cors()
  desired_state.mutable_cors().push_back(CorsEntry{
      google::cloud::internal::optional<std::int64_t>(86400), {"GET"}, {}, {}});

  // default_acl()
  desired_state.mutable_default_acl().push_back(
      ObjectAccessControl()
          .set_entity("allAuthenticatedUsers")
          .set_role("READER"));

  // encryption()
  // TODO(#1003) - need a valid KMS entry to set the encryption.

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
  if (not desired_state.has_versioning()) {
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

  BucketMetadata patched =
      client.PatchBucket(bucket_name, insert_meta, desired_state);
  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_EQ(1U, std::count_if(patched.acl().begin(), patched.acl().end(),
                              [](BucketAccessControl const& x) {
                                return x.entity() == "allAuthenticatedUsers";
                              }));

  // billing()
  EXPECT_EQ(desired_state.billing_as_optional(), patched.billing_as_optional());

  // cors()
  EXPECT_EQ(desired_state.cors(), patched.cors());

  // default_acl() - cannot compare for equality because many fields are updated
  // with unknown values (entity_id, etag, etc)
  EXPECT_EQ(1U, std::count_if(patched.default_acl().begin(),
                              patched.default_acl().end(),
                              [](ObjectAccessControl const& x) {
                                return x.entity() == "allAuthenticatedUsers";
                              }));

  // encryption() - TODO(#1003) - verify the key was correctly used.

  // lifecycle()
  EXPECT_EQ(desired_state.lifecycle_as_optional(),
            patched.lifecycle_as_optional());

  // location()
  EXPECT_EQ(desired_state.location(), patched.location());

  // logging()
  EXPECT_EQ(desired_state.logging_as_optional(), patched.logging_as_optional())
      << patched;

  // storage_class()
  EXPECT_EQ(desired_state.storage_class(), patched.storage_class());

  // versioning()
  EXPECT_EQ(desired_state.versioning(), patched.versioning());

  // website()
  EXPECT_EQ(desired_state.website_as_optional(), patched.website_as_optional());

  client.DeleteBucket(bucket_name);
  client.DeleteBucket(logging_name);
}

TEST_F(BucketIntegrationTest, GetMetadata) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());
}

TEST_F(BucketIntegrationTest, GetMetadataFields) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name, Fields("name"));
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_TRUE(metadata.id().empty());
  EXPECT_TRUE(metadata.kind().empty());
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationMatch_Success) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());

  auto metadata2 = client.GetBucketMetadata(
      bucket_name, storage::Projection("noAcl"),
      storage::IfMetagenerationMatch(metadata.metageneration()));
  EXPECT_EQ(metadata2, metadata);
}

TEST_F(BucketIntegrationTest, GetMetadataIfMetagenerationNotMatch_Failure) {
  auto bucket_name = BucketTestEnvironment::bucket_name();
  Client client;

  auto metadata = client.GetBucketMetadata(bucket_name);
  EXPECT_EQ(bucket_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.id());
  EXPECT_EQ("storage#bucket", metadata.kind());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      client.GetBucketMetadata(
          bucket_name, storage::Projection("noAcl"),
          storage::IfMetagenerationNotMatch(metadata.metageneration())),
      std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      client.GetBucketMetadata(
          bucket_name, storage::Projection("noAcl"),
          storage::IfMetagenerationNotMatch(metadata.metageneration())),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(BucketIntegrationTest, AccessControlCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  Client client;

  // Create a new bucket to run the test, with the "private" PredefinedAcl so
  // we know what the contents of the ACL will be.
  auto meta = client.CreateBucketForProject(
      bucket_name, project_id, BucketMetadata(), PredefinedAcl("private"),
      Projection("full"));

  auto entity_name = MakeEntityName();

  auto name_counter = [](std::string const& name,
                         std::vector<BucketAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](BucketAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_FALSE(meta.acl().empty())
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_EQ(0, name_counter(entity_name, meta.acl()))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << entity_name
      << "> in its ACL.  This is unexpected because the bucket was just"
      << " created with a predefine ACL which should preclude this result.";

  BucketAccessControl result =
      client.CreateBucketAcl(bucket_name, entity_name, "OWNER");
  EXPECT_EQ("OWNER", result.role());
  auto current_acl = client.ListBucketAcl(bucket_name);
  EXPECT_FALSE(current_acl.empty());
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result.entity(), current_acl));

  BucketAccessControl get_result =
      client.GetBucketAcl(bucket_name, entity_name);
  EXPECT_EQ(get_result, result);

  BucketAccessControl new_acl = get_result;
  new_acl.set_role("READER");
  auto updated_result = client.UpdateBucketAcl(bucket_name, new_acl);
  EXPECT_EQ(updated_result.role(), "READER");

  get_result = client.GetBucketAcl(bucket_name, entity_name);
  EXPECT_EQ(get_result, updated_result);

  new_acl = get_result;
  new_acl.set_role("OWNER");
  get_result = client.PatchBucketAcl(bucket_name, entity_name, get_result,
                                     new_acl, IfMatchEtag(get_result.etag()));
  EXPECT_EQ(get_result.role(), new_acl.role());

  client.DeleteBucketAcl(bucket_name, entity_name);
  current_acl = client.ListBucketAcl(bucket_name);
  EXPECT_EQ(0, name_counter(result.entity(), current_acl));

  client.DeleteBucket(bucket_name);
}

TEST_F(BucketIntegrationTest, DefaultObjectAccessControlCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  Client client;

  // Create a new bucket to run the test, with the "private"
  // PredefinedDefaultObjectAcl, that way we can predict the the contents of the
  // ACL.
  auto meta = client.CreateBucketForProject(
      bucket_name, project_id, BucketMetadata(),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"));

  auto entity_name = MakeEntityName();

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](ObjectAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_FALSE(meta.default_acl().empty())
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_EQ(0, name_counter(entity_name, meta.default_acl()))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << entity_name
      << "> in its ACL.  This is unexpected because the bucket was just"
      << " created with a predefine ACL which should preclude this result.";

  ObjectAccessControl result =
      client.CreateDefaultObjectAcl(bucket_name, entity_name, "OWNER");
  EXPECT_EQ("OWNER", result.role());
  auto current_acl = client.ListDefaultObjectAcl(bucket_name);
  EXPECT_FALSE(current_acl.empty());
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result.entity(), current_acl));

  auto get_result = client.GetDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_EQ(get_result, result);

  ObjectAccessControl new_acl = get_result;
  new_acl.set_role("READER");
  auto updated_result = client.UpdateDefaultObjectAcl(bucket_name, new_acl);
  EXPECT_EQ(updated_result.role(), "READER");
  get_result = client.GetDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_EQ(get_result, updated_result);

  new_acl = get_result;
  new_acl.set_role("OWNER");
  get_result =
      client.PatchDefaultObjectAcl(bucket_name, entity_name, get_result,
                                   new_acl, IfMatchEtag(get_result.etag()));
  EXPECT_EQ(get_result.role(), new_acl.role());

  client.DeleteDefaultObjectAcl(bucket_name, entity_name);
  current_acl = client.ListDefaultObjectAcl(bucket_name);
  EXPECT_EQ(0, name_counter(result.entity(), current_acl));

  client.DeleteBucket(bucket_name);
}

TEST_F(BucketIntegrationTest, NotificationsCRUD) {
  auto project_id = BucketTestEnvironment::project_id();
  std::string bucket_name = MakeRandomBucketName();
  Client client;

  // Create a new bucket to run the test.
  auto meta =
      client.CreateBucketForProject(bucket_name, project_id, BucketMetadata());

  auto current_notifications = client.ListNotifications(bucket_name);
  EXPECT_TRUE(current_notifications.empty())
      << "Test aborted. Non-empty notification list returned from newly"
      << " created bucket <" << bucket_name
      << ">. This is unexpected because the bucket name is chosen at random.";

  auto create = client.CreateNotification(
      bucket_name, BucketTestEnvironment::topic(), payload_format::JsonApiV1(),
      NotificationMetadata().append_event_type(event_type::ObjectFinalize()));

  EXPECT_EQ(payload_format::JsonApiV1(), create.payload_format());
  EXPECT_THAT(create.topic(), HasSubstr(BucketTestEnvironment::topic()));

  current_notifications = client.ListNotifications(bucket_name);
  auto count =
      std::count_if(current_notifications.begin(), current_notifications.end(),
                    [create](NotificationMetadata const& x) {
                      return x.id() == create.id();
                    });
  EXPECT_EQ(1U, count) << create;
  std::cout << create << std::endl;

  client.DeleteBucket(bucket_name);
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
              << " <project> <bucket> <topic>" << std::endl;
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
