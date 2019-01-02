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
#include "google/cloud/testing_util/expect_exception.h"
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

class ObjectIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + ObjectTestEnvironment::project_id();
  }
};

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(ObjectIntegrationTest, BasicCRUD) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();

  auto objects = client.ListObjects(bucket_name);
  std::vector<ObjectMetadata> initial_list(objects.begin(), objects.end());

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectMetadata> const& list) {
    return std::count_if(
        list.begin(), list.end(),
        [name](ObjectMetadata const& m) { return m.name() == name; });
  };

  auto object_name = MakeRandomObjectName();
  ASSERT_EQ(0, name_counter(object_name, initial_list))
      << "Test aborted. The object <" << object_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  // Create the object, but only if it does not exist already.
  ObjectMetadata insert_meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection("full"));
  objects = client.ListObjects(bucket_name);

  std::vector<ObjectMetadata> current_list(objects.begin(), objects.end());
  EXPECT_EQ(1U, name_counter(object_name, current_list));

  ObjectMetadata get_meta = client.GetObjectMetadata(
      bucket_name, object_name, Generation(insert_meta.generation()),
      Projection("full"));
  EXPECT_EQ(get_meta, insert_meta);

  ObjectMetadata update = get_meta;
  update.mutable_acl().emplace_back(
      ObjectAccessControl().set_role("READER").set_entity(
          "allAuthenticatedUsers"));
  update.set_cache_control("no-cache")
      .set_content_disposition("inline")
      .set_content_encoding("identity")
      .set_content_language("en")
      .set_content_type("plain/text");
  update.mutable_metadata().emplace("updated", "true");
  ObjectMetadata updated_meta =
      client.UpdateObject(bucket_name, object_name, update, Projection("full"));

  // Because some of the ACL values are not predictable we convert the values we
  // care about to strings and compare that.
  {
    auto acl_to_string_vector =
        [](std::vector<ObjectAccessControl> const& acl) {
          std::vector<std::string> v;
          std::transform(acl.begin(), acl.end(), std::back_inserter(v),
                         [](ObjectAccessControl const& x) {
                           return x.entity() + " = " + x.role();
                         });
          return v;
        };
    auto expected = acl_to_string_vector(update.acl());
    auto actual = acl_to_string_vector(updated_meta.acl());
    EXPECT_THAT(expected, ::testing::UnorderedElementsAreArray(actual));
  }
  EXPECT_EQ(update.cache_control(), updated_meta.cache_control())
      << updated_meta;
  EXPECT_EQ(update.content_disposition(), updated_meta.content_disposition())
      << updated_meta;
  EXPECT_EQ(update.content_encoding(), updated_meta.content_encoding())
      << updated_meta;
  EXPECT_EQ(update.content_language(), updated_meta.content_language())
      << updated_meta;
  EXPECT_EQ(update.content_type(), updated_meta.content_type()) << updated_meta;
  EXPECT_EQ(update.metadata(), updated_meta.metadata()) << updated_meta;

  ObjectMetadata desired_patch = updated_meta;
  desired_patch.set_content_language("en");
  desired_patch.mutable_metadata().erase("updated");
  desired_patch.mutable_metadata().emplace("patched", "true");
  ObjectMetadata patched_meta =
      client.PatchObject(bucket_name, object_name, updated_meta, desired_patch);
  EXPECT_EQ(desired_patch.metadata(), patched_meta.metadata()) << patched_meta;
  EXPECT_EQ(desired_patch.content_language(), patched_meta.content_language())
      << patched_meta;

  client.DeleteObject(bucket_name, object_name);
  objects = client.ListObjects(bucket_name);
  current_list.assign(objects.begin(), objects.end());

  EXPECT_EQ(0U, name_counter(object_name, current_list));
}

TEST_F(ObjectIntegrationTest, FullPatch) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  // Create the object, but only if it does not exist already.
  ObjectMetadata original =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection("full"));

  ObjectMetadata desired = original;
  desired.mutable_acl().push_back(ObjectAccessControl()
                                      .set_entity("allAuthenticatedUsers")
                                      .set_role("READER"));
  if (original.cache_control() != "no-cache") {
    desired.set_cache_control("no-cache");
  } else {
    desired.set_cache_control("");
  }
  if (original.content_disposition() != "inline") {
    desired.set_content_disposition("inline");
  } else {
    desired.set_content_disposition("attachment; filename=test.txt");
  }
  if (original.content_encoding() != "identity") {
    desired.set_content_encoding("identity");
  } else {
    desired.set_content_encoding("");
  }
  // Use 'en' and 'fr' as test languages because they are known to be supported.
  // The server rejects private tags such as 'x-pig-latin'.
  if (original.content_language() != "en") {
    desired.set_content_language("en");
  } else {
    desired.set_content_language("fr");
  }
  if (original.content_type() != "application/octet-stream") {
    desired.set_content_type("application/octet-stream");
  } else {
    desired.set_content_type("application/text");
  }

  // We want to create a diff that modifies the metadata, so either erase or
  // insert a value for `test-label` depending on the initial state.
  if (original.has_metadata("test-label")) {
    desired.mutable_metadata().erase("test-label");
  } else {
    desired.mutable_metadata().emplace("test-label", "test-value");
  }

  ObjectMetadata patched =
      client.PatchObject(bucket_name, object_name, original, desired);

  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_EQ(1U, std::count_if(patched.acl().begin(), patched.acl().end(),
                              [](ObjectAccessControl const& x) {
                                return x.entity() == "allAuthenticatedUsers";
                              }));

  EXPECT_EQ(desired.cache_control(), patched.cache_control());
  EXPECT_EQ(desired.content_disposition(), patched.content_disposition());
  EXPECT_EQ(desired.content_encoding(), patched.content_encoding());
  EXPECT_EQ(desired.content_language(), patched.content_language());
  EXPECT_EQ(desired.content_type(), patched.content_type());
  EXPECT_EQ(desired.metadata(), patched.metadata());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ListObjectsVersions) {
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  Client client;

  // This test requires the bucket to be configured with versioning. The buckets
  // used by the CI build are already configured with versioning enabled. The
  // bucket created in the testbench also has versioning. Regardless, check here
  // first to produce a better error message if there is a configuration
  // problem.
  auto bucket_meta = client.GetBucketMetadata(bucket_name);
  ASSERT_TRUE(bucket_meta.versioning().has_value());
  ASSERT_TRUE(bucket_meta.versioning().value().enabled);

  auto create_object_with_3_versions = [&client, &bucket_name, this] {
    auto object_name = MakeRandomObjectName();
    auto meta = client.InsertObject(bucket_name, object_name,
                                    "contents for the first revision",
                                    storage::IfGenerationMatch(0));
    client.InsertObject(bucket_name, object_name,
                        "contents for the second revision");
    client.InsertObject(bucket_name, object_name,
                        "contents for the final revision");
    return meta.name();
  };

  std::vector<std::string> expected(4);
  std::generate_n(expected.begin(), expected.size(),
                  create_object_with_3_versions);

  ListObjectsReader reader = client.ListObjects(bucket_name, Versions(true));
  std::vector<std::string> actual;
  for (auto it = reader.begin(); it != reader.end(); ++it) {
    auto const& meta = *it;
    EXPECT_EQ(bucket_name, meta.bucket());
    actual.push_back(meta.name());
  }
  auto produce_joined_list = [&actual] {
    std::string joined;
    for (auto const& x : actual) {
      joined += "  ";
      joined += x;
      joined += "\n";
    }
    return joined;
  };
  // There may be a lot of other objects in the bucket, so we want to verify
  // that any objects we created are found there, but cannot expect a perfect
  // match.

  for (auto const& name : expected) {
    EXPECT_EQ(3, std::count(actual.begin(), actual.end(), name))
        << "Expected to find 3 copies of " << name << " in the object list:\n"
        << produce_joined_list();
  }
}

TEST_F(ObjectIntegrationTest, BasicReadWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name, expected,
                                            IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, EncryptedReadWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();
  EncryptionKeyData key = MakeEncryptionKeyData();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, expected,
                          IfGenerationMatch(0), EncryptionKey(key));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  ASSERT_TRUE(meta.has_customer_encryption());
  EXPECT_EQ("AES256", meta.customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta.customer_encryption().key_sha256);

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name, EncryptionKey(key));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ReadNotFound) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  EXPECT_FALSE(stream.status().ok());
  EXPECT_FALSE(stream.IsOpen());
  EXPECT_EQ(404, stream.status().status_code()) << "status=" << stream.status();
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectIntegrationTest, Copy) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto source_object_name = MakeRandomObjectName();
  auto destination_object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  ObjectMetadata source_meta = client.InsertObject(
      bucket_name, source_object_name, expected, IfGenerationMatch(0));
  EXPECT_EQ(source_object_name, source_meta.name());
  EXPECT_EQ(bucket_name, source_meta.bucket());

  ObjectMetadata meta = client.CopyObject(
      bucket_name, source_object_name, bucket_name, destination_object_name,
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));
  EXPECT_EQ(destination_object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());

  auto stream = client.ReadObject(bucket_name, destination_object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, destination_object_name);
  client.DeleteObject(bucket_name, source_object_name);
}

TEST_F(ObjectIntegrationTest, StreamingWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, StreamingWriteAutoClose) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::string expected = "A short string to test\n";

  {
    // Create the object, but only if it does not exist already.
    auto os =
        client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
    os << expected;
  }
  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, XmlStreamingWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               Fields(""));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  // When asking for an empty list of fields we should not expect any values:
  EXPECT_TRUE(meta.bucket().empty());
  EXPECT_TRUE(meta.name().empty());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, XmlReadWrite) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name, expected,
                                            IfGenerationMatch(0), Fields(""));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, AccessControlCRUD) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  (void)client.InsertObject(bucket_name, object_name, LoremIpsum(),
                            IfGenerationMatch(0));

  auto entity_name = MakeEntityName();
  std::vector<ObjectAccessControl> initial_acl =
      client.ListObjectAcl(bucket_name, object_name);

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](ObjectAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_EQ(0, name_counter(entity_name, initial_acl))
      << "Test aborted. The entity <" << entity_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  ObjectAccessControl result =
      client.CreateObjectAcl(bucket_name, object_name, entity_name, "OWNER");
  EXPECT_EQ("OWNER", result.role());
  auto current_acl = client.ListObjectAcl(bucket_name, object_name);
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result.entity(), current_acl));

  auto get_result = client.GetObjectAcl(bucket_name, object_name, entity_name);
  EXPECT_EQ(get_result, result);

  ObjectAccessControl new_acl = get_result;
  new_acl.set_role("READER");
  auto updated_result =
      client.UpdateObjectAcl(bucket_name, object_name, new_acl);
  EXPECT_EQ(updated_result.role(), "READER");
  get_result = client.GetObjectAcl(bucket_name, object_name, entity_name);
  EXPECT_EQ(get_result, updated_result);

  new_acl = get_result;
  new_acl.set_role("OWNER");
  // Because this is a freshly created object, with a random name, we do not
  // worry about implementing optimistic concurrency control.
  get_result = client.PatchObjectAcl(bucket_name, object_name, entity_name,
                                     get_result, new_acl);
  EXPECT_EQ(get_result.role(), new_acl.role());

  // Remove an entity and verify it is no longer in the ACL.
  client.DeleteObjectAcl(bucket_name, object_name, entity_name);
  current_acl = client.ListObjectAcl(bucket_name, object_name);
  EXPECT_EQ(0, name_counter(result.entity(), current_acl));

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, WriteWithContentType) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               ContentType("text/plain"));
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyPredefinedAclAuthenticatedRead) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  ObjectMetadata original = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ObjectMetadata meta = client.CopyObject(
      bucket_name, object_name, bucket_name, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::AuthenticatedRead(), Projection::Full());
  EXPECT_LT(0, CountMatchingEntities(meta.acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << meta;

  client.DeleteObject(bucket_name, copy_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyPredefinedAclBucketOwnerFullControl) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  BucketMetadata bucket =
      client.GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_TRUE(bucket.has_owner());
  std::string owner = bucket.owner().entity;

  ObjectMetadata original = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ObjectMetadata meta = client.CopyObject(
      bucket_name, object_name, bucket_name, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::BucketOwnerFullControl(), Projection::Full());
  EXPECT_LT(0, CountMatchingEntities(
                   meta.acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << meta;

  client.DeleteObject(bucket_name, copy_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyPredefinedAclBucketOwnerRead) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  BucketMetadata bucket =
      client.GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_TRUE(bucket.has_owner());
  std::string owner = bucket.owner().entity;

  ObjectMetadata original = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ObjectMetadata meta = client.CopyObject(
      bucket_name, object_name, bucket_name, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::BucketOwnerRead(), Projection::Full());
  EXPECT_LT(0, CountMatchingEntities(
                   meta.acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << meta;

  client.DeleteObject(bucket_name, copy_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyPredefinedAclPrivate) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  ObjectMetadata original = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ObjectMetadata meta = client.CopyObject(
      bucket_name, object_name, bucket_name, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::Private(), Projection::Full());
  ASSERT_TRUE(meta.has_owner());
  EXPECT_LT(
      0, CountMatchingEntities(meta.acl(), ObjectAccessControl()
                                               .set_entity(meta.owner().entity)
                                               .set_role("OWNER")))
      << meta;

  client.DeleteObject(bucket_name, copy_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyPredefinedAclProjectPrivate) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  ObjectMetadata original = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ObjectMetadata meta = client.CopyObject(
      bucket_name, object_name, bucket_name, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::ProjectPrivate(), Projection::Full());
  ASSERT_TRUE(meta.has_owner());
  EXPECT_LT(
      0, CountMatchingEntities(meta.acl(), ObjectAccessControl()
                                               .set_entity(meta.owner().entity)
                                               .set_role("OWNER")))
      << meta;

  client.DeleteObject(bucket_name, copy_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyPredefinedAclPublicRead) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto copy_name = MakeRandomObjectName();

  ObjectMetadata original = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ObjectMetadata meta = client.CopyObject(
      bucket_name, object_name, bucket_name, copy_name, IfGenerationMatch(0),
      DestinationPredefinedAcl::PublicRead(), Projection::Full());
  EXPECT_LT(
      0, CountMatchingEntities(
             meta.acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << meta;

  client.DeleteObject(bucket_name, copy_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ComposeSimple) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name,
                                            LoremIpsum(), IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());

  // Compose new of object using previously created object
  auto composed_object_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> source_objects = {{object_name},
                                                     {object_name}};
  ObjectMetadata composed_meta = client.ComposeObject(
      bucket_name, source_objects, composed_object_name,
      WithObjectMetadata(ObjectMetadata().set_content_type("plain/text")));

  EXPECT_EQ(meta.size() * 2, composed_meta.size());
  client.DeleteObject(bucket_name, composed_object_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ComposedUsingEncryptedObject) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string content = LoremIpsum();
  EncryptionKeyData key = MakeEncryptionKeyData();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, content,
                          IfGenerationMatch(0), EncryptionKey(key));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  ASSERT_TRUE(meta.has_customer_encryption());
  EXPECT_EQ("AES256", meta.customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta.customer_encryption().key_sha256);

  // Compose new of object using previously created object
  auto composed_object_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> source_objects = {{object_name},
                                                     {object_name}};
  ObjectMetadata composed_meta = client.ComposeObject(
      bucket_name, source_objects, composed_object_name, EncryptionKey(key));

  EXPECT_EQ(meta.size() * 2, composed_meta.size());
  client.DeleteObject(bucket_name, composed_object_name);
  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, RewriteSimple) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto source_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  ObjectMetadata source_meta = client.InsertObject(
      bucket_name, source_name, LoremIpsum(), IfGenerationMatch(0));
  EXPECT_EQ(source_name, source_meta.name());
  EXPECT_EQ(bucket_name, source_meta.bucket());

  // Rewrite object into a new object.
  auto object_name = MakeRandomObjectName();
  ObjectMetadata rewritten_meta = client.RewriteObjectBlocking(
      bucket_name, source_name, bucket_name, object_name);

  EXPECT_EQ(bucket_name, rewritten_meta.bucket());
  EXPECT_EQ(object_name, rewritten_meta.name());

  client.DeleteObject(bucket_name, object_name);
  client.DeleteObject(bucket_name, source_name);
}

TEST_F(ObjectIntegrationTest, RewriteEncrypted) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto source_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  EncryptionKeyData source_key = MakeEncryptionKeyData();
  ObjectMetadata source_meta =
      client.InsertObject(bucket_name, source_name, LoremIpsum(),
                          IfGenerationMatch(0), EncryptionKey(source_key));
  EXPECT_EQ(source_name, source_meta.name());
  EXPECT_EQ(bucket_name, source_meta.bucket());

  // Compose new of object using previously created object
  auto object_name = MakeRandomObjectName();
  EncryptionKeyData dest_key = MakeEncryptionKeyData();
  ObjectRewriter rewriter = client.RewriteObject(
      bucket_name, source_name, bucket_name, object_name,
      SourceEncryptionKey(source_key), EncryptionKey(dest_key));

  ObjectMetadata rewritten_meta = rewriter.Result();
  EXPECT_EQ(bucket_name, rewritten_meta.bucket());
  EXPECT_EQ(object_name, rewritten_meta.name());

  client.DeleteObject(bucket_name, object_name);
  client.DeleteObject(bucket_name, source_name);
}

TEST_F(ObjectIntegrationTest, RewriteLarge) {
  // The testbench always requires multiple iterations to copy this object.
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto source_name = MakeRandomObjectName();

  std::string large_text;
  long const lines = 8 * 1024 * 1024 / 128;
  for (long i = 0; i != lines; ++i) {
    auto line = google::cloud::internal::Sample(generator_, 127,
                                                "abcdefghijklmnopqrstuvwxyz"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "012456789");
    large_text += line + "\n";
  }

  ObjectMetadata source_meta = client.InsertObject(
      bucket_name, source_name, large_text, IfGenerationMatch(0));
  EXPECT_EQ(source_name, source_meta.name());
  EXPECT_EQ(bucket_name, source_meta.bucket());

  // Rewrite object into a new object.
  auto object_name = MakeRandomObjectName();
  ObjectRewriter writer =
      client.RewriteObject(bucket_name, source_name, bucket_name, object_name);

  ObjectMetadata rewritten_meta =
      writer.ResultWithProgressCallback([](RewriteProgress const& p) {
        EXPECT_TRUE((p.total_bytes_rewritten < p.object_size) xor p.done)
            << "p.done=" << p.done << ", p.object_size=" << p.object_size
            << ", p.total_bytes_rewritten=" << p.total_bytes_rewritten;
      });

  EXPECT_EQ(bucket_name, rewritten_meta.bucket());
  EXPECT_EQ(object_name, rewritten_meta.name());

  client.DeleteObject(bucket_name, object_name);
  client.DeleteObject(bucket_name, source_name);
}

/// @test Verify that MD5 hashes are computed by default.
TEST_F(ObjectIntegrationTest, DefaultMD5HashXML) {
  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  ObjectMetadata insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0), Fields(""));
  LogSink::Instance().RemoveBackend(id);

  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [](std::string const& line) {
                      return line.rfind("x-goog-hash: md5=", 0) == 0;
                    });
  EXPECT_EQ(1, count);

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hashes are computed by default.
TEST_F(ObjectIntegrationTest, DefaultMD5HashJSON) {
  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  ObjectMetadata insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [](std::string const& line) {
        // This is a big indirect, we detect if the upload changed to
        // multipart/related, and if so, we assume the hash value is being used.
        // Unfortunately I (@coryan) cannot think of a way to examine the upload
        // contents.
        return line.rfind("content-type: multipart/related; boundary=", 0) == 0;
      });
  EXPECT_EQ(1, count);

  if (insert_meta.has_metadata("x_testbench_upload")) {
    // When running against the testbench, we have some more information to
    // verify the right upload type and contents were sent.
    EXPECT_EQ("multipart", insert_meta.metadata("x_testbench_upload"));
    ASSERT_TRUE(insert_meta.has_metadata("x_testbench_md5"));
    auto expected_md5 = ComputeMD5Hash(LoremIpsum());
    EXPECT_EQ(expected_md5, insert_meta.metadata("x_testbench_md5"));
  }

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that `DisableMD5Hash` actually disables the header.
TEST_F(ObjectIntegrationTest, DisableMD5HashXML) {
  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  ObjectMetadata insert_meta = client.InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0),
      DisableMD5Hash(true), Fields(""));
  LogSink::Instance().RemoveBackend(id);

  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [](std::string const& line) {
                      return line.rfind("x-goog-hash: md5=", 0) == 0;
                    });
  EXPECT_EQ(0U, count);

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that `DisableMD5Hash` actually disables the payload.
TEST_F(ObjectIntegrationTest, DisableMD5HashJSON) {
  Client client(ClientOptions()
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  ObjectMetadata insert_meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), DisableMD5Hash(true));
  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [](std::string const& line) {
        // This is a big indirect, we detect if the upload changed to
        // multipart/related, and if so, we assume the hash value is being used.
        // Unfortunately I (@coryan) cannot think of a way to examine the upload
        // contents.
        return line.rfind("content-type: multipart/related; boundary=", 0) == 0;
      });
  EXPECT_EQ(0, count);

  if (insert_meta.has_metadata("x_testbench_upload")) {
    // When running against the testbench, we have some more information to
    // verify the right upload type and contents were sent.
    EXPECT_EQ("simple", insert_meta.metadata("x_testbench_upload"));
    ASSERT_FALSE(insert_meta.has_metadata("x_testbench_md5"));
  }

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hashes are computed by default on downloads.
TEST_F(ObjectIntegrationTest, DefaultMD5StreamingReadXML) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta.md5_hash()));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hashes are computed by default on downloads.
TEST_F(ObjectIntegrationTest, DefaultMD5StreamingReadJSON) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream =
      client.ReadObject(bucket_name, object_name, IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta.md5_hash()));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that hashes and checksums can be disabled on downloads.
TEST_F(ObjectIntegrationTest, DisableHashesStreamingReadXML) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream =
      client.ReadObject(bucket_name, object_name, DisableMD5Hash(true),
                        DisableCrc32cChecksum(true));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_TRUE(stream.computed_hash().empty());
  EXPECT_TRUE(stream.received_hash().empty());

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that hashes and checksums can be disabled on downloads.
TEST_F(ObjectIntegrationTest, DisableHashesStreamingReadJSON) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  ObjectMetadata meta =
      client.InsertObject(bucket_name, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection::Full());
  auto stream = client.ReadObject(
      bucket_name, object_name, DisableMD5Hash(true),
      DisableCrc32cChecksum(true), IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_TRUE(stream.computed_hash().empty());
  EXPECT_TRUE(stream.received_hash().empty());

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hashes are computed by default on uploads.
TEST_F(ObjectIntegrationTest, DefaultMD5StreamingWriteXML) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               Fields(""));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_md5hash = ComputeMD5Hash(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_md5hash));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that MD5 hashes are computed by default on uploads.
TEST_F(ObjectIntegrationTest, DefaultMD5StreamingWriteJSON) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_md5hash = ComputeMD5Hash(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_md5hash));

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that hashes and checksums can be disabled in uploads.
TEST_F(ObjectIntegrationTest, DisableHashesStreamingWriteXML) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                               Fields(""), DisableMD5Hash(true),
                               DisableCrc32cChecksum(true));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_TRUE(os.received_hash().empty());
  EXPECT_TRUE(os.computed_hash().empty());

  client.DeleteObject(bucket_name, object_name);
}

/// @test Verify that hashes and checksums can be disabled in uploads.
TEST_F(ObjectIntegrationTest, DisableHashesStreamingWriteJSON) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os =
      client.WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                         DisableMD5Hash(true), DisableCrc32cChecksum(true));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_TRUE(os.received_hash().empty());
  EXPECT_TRUE(os.computed_hash().empty());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, CopyFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto source_object_name = MakeRandomObjectName();
  auto destination_object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.CopyObject(bucket_name, source_object_name, bucket_name,
                      destination_object_name);
  });
}

TEST_F(ObjectIntegrationTest, GetObjectMetadataFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure(
      [&] { client.GetObjectMetadata(bucket_name, object_name); });
}

TEST_F(ObjectIntegrationTest, StreamingWriteFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name, expected,
                                            IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());

  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::badbit | std::ios_base::failbit);
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  testing_util::ExpectException<std::ios::failure>(
      [&] { os.Close(); },
      [&](std::ios::failure const& ex) {
        EXPECT_FALSE(os.metadata().ok());
        EXPECT_EQ(412, os.metadata().status().status_code());
      },
      "" /* the message generated by the C++ runtime is unknown */);

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, StreamingWriteFailureNoex) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  ObjectMetadata meta = client.InsertObject(bucket_name, object_name, expected,
                                            IfGenerationMatch(0));
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());

  auto os = client.WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().ok());
  EXPECT_EQ(412, os.metadata().status().status_code());

  client.DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ListObjectsFailure) {
  auto bucket_name = MakeRandomBucketName();
  Client client;

  ListObjectsReader reader = client.ListObjects(bucket_name, Versions(true));

  // This operation should fail because the bucket does not exist.
  TestPermanentFailure([&] {
    std::vector<ObjectMetadata> actual;
    actual.assign(reader.begin(), reader.end());
  });
}

TEST_F(ObjectIntegrationTest, DeleteObjectFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] { client.DeleteObject(bucket_name, object_name); });
}

TEST_F(ObjectIntegrationTest, UpdateObjectFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure(
      [&] { client.UpdateObject(bucket_name, object_name, ObjectMetadata()); });
}

TEST_F(ObjectIntegrationTest, PatchObjectFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.PatchObject(bucket_name, object_name, ObjectMetadataPatchBuilder());
  });
}

TEST_F(ObjectIntegrationTest, ComposeFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto composed_object_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> source_objects = {{object_name},
                                                     {object_name}};

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.ComposeObject(bucket_name, source_objects, composed_object_name);
  });
}

TEST_F(ObjectIntegrationTest, RewriteFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto source_object_name = MakeRandomObjectName();
  auto destination_object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.RewriteObjectBlocking(bucket_name, source_object_name, bucket_name,
                                 destination_object_name);
  });
}

TEST_F(ObjectIntegrationTest, ListAccessControlFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] { client.ListObjectAcl(bucket_name, object_name); });
}

TEST_F(ObjectIntegrationTest, CreateAccessControlFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.CreateObjectAcl(bucket_name, object_name, entity_name, "READER");
  });
}

TEST_F(ObjectIntegrationTest, GetAccessControlFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure(
      [&] { client.GetObjectAcl(bucket_name, object_name, entity_name); });
}

TEST_F(ObjectIntegrationTest, UpdateAccessControlFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.UpdateObjectAcl(
        bucket_name, object_name,
        ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  });
}

TEST_F(ObjectIntegrationTest, PatchAccessControlFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure([&] {
    client.PatchObjectAcl(
        bucket_name, object_name, entity_name, ObjectAccessControl(),
        ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  });
}

TEST_F(ObjectIntegrationTest, DeleteAccessControlFailure) {
  Client client;
  auto bucket_name = ObjectTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  TestPermanentFailure(
      [&] { client.DeleteObjectAcl(bucket_name, object_name, entity_name); });
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
