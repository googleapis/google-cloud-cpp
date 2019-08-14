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
#include "google/cloud/testing_util/assert_ok.h"
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

using ::google::cloud::storage::testing::CountMatchingEntities;
using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_project_id;
char const* flag_bucket_name;

class ObjectIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + std::string(flag_project_id);
  }
};

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(ObjectIntegrationTest, BasicCRUD) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;

  auto objects = client->ListObjects(bucket_name);
  std::vector<ObjectMetadata> initial_list;
  for (auto&& o : objects) {
    initial_list.emplace_back(std::move(o).value());
  }

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
  StatusOr<ObjectMetadata> insert_meta =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);

  objects = client->ListObjects(bucket_name);

  std::vector<ObjectMetadata> current_list;
  for (auto&& o : objects) {
    current_list.emplace_back(std::move(o).value());
  }
  EXPECT_EQ(1, name_counter(object_name, current_list));

  StatusOr<ObjectMetadata> get_meta = client->GetObjectMetadata(
      bucket_name, object_name, Generation(insert_meta->generation()),
      Projection("full"));
  ASSERT_STATUS_OK(get_meta);
  EXPECT_EQ(*get_meta, *insert_meta);

  ObjectMetadata update = *get_meta;
  update.mutable_acl().emplace_back(
      ObjectAccessControl().set_role("READER").set_entity(
          "allAuthenticatedUsers"));
  update.set_cache_control("no-cache")
      .set_content_disposition("inline")
      .set_content_encoding("identity")
      .set_content_language("en")
      .set_content_type("plain/text");
  update.mutable_metadata().emplace("updated", "true");
  StatusOr<ObjectMetadata> updated_meta = client->UpdateObject(
      bucket_name, object_name, update, Projection("full"));
  ASSERT_STATUS_OK(updated_meta);

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
    auto actual = acl_to_string_vector(updated_meta->acl());
    EXPECT_THAT(expected, ::testing::UnorderedElementsAreArray(actual));
  }
  EXPECT_EQ(update.cache_control(), updated_meta->cache_control())
      << *updated_meta;
  EXPECT_EQ(update.content_disposition(), updated_meta->content_disposition())
      << *updated_meta;
  EXPECT_EQ(update.content_encoding(), updated_meta->content_encoding())
      << *updated_meta;
  EXPECT_EQ(update.content_language(), updated_meta->content_language())
      << *updated_meta;
  EXPECT_EQ(update.content_type(), updated_meta->content_type())
      << *updated_meta;
  EXPECT_EQ(update.metadata(), updated_meta->metadata()) << *updated_meta;

  ObjectMetadata desired_patch = *updated_meta;
  desired_patch.set_content_language("en");
  desired_patch.mutable_metadata().erase("updated");
  desired_patch.mutable_metadata().emplace("patched", "true");
  StatusOr<ObjectMetadata> patched_meta = client->PatchObject(
      bucket_name, object_name, *updated_meta, desired_patch);
  ASSERT_STATUS_OK(patched_meta);

  EXPECT_EQ(desired_patch.metadata(), patched_meta->metadata())
      << *patched_meta;
  EXPECT_EQ(desired_patch.content_language(), patched_meta->content_language())
      << *patched_meta;

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);

  objects = client->ListObjects(bucket_name);
  current_list.clear();
  for (auto&& o : objects) {
    current_list.emplace_back(std::move(o).value());
  }

  EXPECT_EQ(0, name_counter(object_name, current_list));
}

TEST_F(ObjectIntegrationTest, FullPatch) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> original =
      client->InsertObject(bucket_name, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(original);

  ObjectMetadata desired = *original;
  desired.mutable_acl().push_back(ObjectAccessControl()
                                      .set_entity("allAuthenticatedUsers")
                                      .set_role("READER"));
  if (original->cache_control() != "no-cache") {
    desired.set_cache_control("no-cache");
  } else {
    desired.set_cache_control("");
  }
  if (original->content_disposition() != "inline") {
    desired.set_content_disposition("inline");
  } else {
    desired.set_content_disposition("attachment; filename=test.txt");
  }
  if (original->content_encoding() != "identity") {
    desired.set_content_encoding("identity");
  } else {
    desired.set_content_encoding("");
  }
  // Use 'en' and 'fr' as test languages because they are known to be supported.
  // The server rejects private tags such as 'x-pig-latin'.
  if (original->content_language() != "en") {
    desired.set_content_language("en");
  } else {
    desired.set_content_language("fr");
  }
  if (original->content_type() != "application/octet-stream") {
    desired.set_content_type("application/octet-stream");
  } else {
    desired.set_content_type("application/text");
  }

  // We want to create a diff that modifies the metadata, so either erase or
  // insert a value for `test-label` depending on the initial state.
  if (original->has_metadata("test-label")) {
    desired.mutable_metadata().erase("test-label");
  } else {
    desired.mutable_metadata().emplace("test-label", "test-value");
  }

  StatusOr<ObjectMetadata> patched =
      client->PatchObject(bucket_name, object_name, *original, desired);
  ASSERT_STATUS_OK(patched);

  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_EQ(1, std::count_if(patched->acl().begin(), patched->acl().end(),
                             [](ObjectAccessControl const& x) {
                               return x.entity() == "allAuthenticatedUsers";
                             }));

  EXPECT_EQ(desired.cache_control(), patched->cache_control());
  EXPECT_EQ(desired.content_disposition(), patched->content_disposition());
  EXPECT_EQ(desired.content_encoding(), patched->content_encoding());
  EXPECT_EQ(desired.content_language(), patched->content_language());
  EXPECT_EQ(desired.content_type(), patched->content_type());
  EXPECT_EQ(desired.metadata(), patched->metadata());

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, ListObjectsVersions) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  // This test requires the bucket to be configured with versioning. The buckets
  // used by the CI build are already configured with versioning enabled. The
  // bucket created in the testbench also has versioning. Regardless, set the
  // bucket to the desired state, which will produce a better error message if
  // there is a configuration problem.
  auto bucket_meta = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(bucket_meta);
  auto updated_meta = client->PatchBucket(
      bucket_name,
      BucketMetadataPatchBuilder().SetVersioning(BucketVersioning{true}),
      IfMetagenerationMatch(bucket_meta->metageneration()));
  ASSERT_STATUS_OK(updated_meta);
  ASSERT_TRUE(updated_meta->versioning().has_value());
  ASSERT_TRUE(updated_meta->versioning().value().enabled);

  auto create_object_with_3_versions = [&client, &bucket_name, this] {
    auto object_name = MakeRandomObjectName();
    // ASSERT_TRUE does not work inside this lambda because the return type is
    // not `void`, use `.value()` instead to throw (or crash) on the unexpected
    // error.
    auto meta = client
                    ->InsertObject(bucket_name, object_name,
                                   "contents for the first revision",
                                   storage::IfGenerationMatch(0))
                    .value();
    client
        ->InsertObject(bucket_name, object_name,
                       "contents for the second revision")
        .value();
    client
        ->InsertObject(bucket_name, object_name,
                       "contents for the final revision")
        .value();
    return meta.name();
  };

  std::vector<std::string> expected(4);
  std::generate_n(expected.begin(), expected.size(),
                  create_object_with_3_versions);

  ListObjectsReader reader = client->ListObjects(bucket_name, Versions(true));
  std::vector<std::string> actual;
  for (auto it = reader.begin(); it != reader.end(); ++it) {
    auto const& meta = it->value();
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
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, EncryptedReadWrite) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();
  EncryptionKeyData key = MakeEncryptionKeyData();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client->InsertObject(bucket_name, object_name, expected,
                           IfGenerationMatch(0), EncryptionKey(key));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());
  ASSERT_TRUE(meta->has_customer_encryption());
  EXPECT_EQ("AES256", meta->customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta->customer_encryption().key_sha256);

  // Create a iostream to read the object back.
  auto stream =
      client->ReadObject(bucket_name, object_name, EncryptionKey(key));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, ReadNotFound) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  EXPECT_FALSE(stream.status().ok());
  EXPECT_FALSE(stream.IsOpen());
  EXPECT_EQ(StatusCode::kNotFound, stream.status().code())
      << "status=" << stream.status();
  EXPECT_TRUE(stream.bad());
}

TEST_F(ObjectIntegrationTest, StreamingWrite) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
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
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteAutoClose) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::string expected = "A short string to test\n";

  {
    // Create the object, but only if it does not exist already.
    auto os =
        client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
    os << expected;
  }
  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteEmpty) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ASSERT_EQ(object_name, meta.name());
  ASSERT_EQ(bucket_name, meta.bucket());
  ASSERT_EQ(0U, meta.size());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_TRUE(actual.empty());

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, XmlStreamingWrite) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
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
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, XmlReadWrite) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, AccessControlCRUD) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto insert = client->InsertObject(bucket_name, object_name, LoremIpsum(),
                                     IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);

  auto entity_name = MakeEntityName();
  StatusOr<std::vector<ObjectAccessControl>> initial_acl =
      client->ListObjectAcl(bucket_name, object_name);
  ASSERT_STATUS_OK(initial_acl);

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](ObjectAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_EQ(0, name_counter(entity_name, *initial_acl))
      << "Test aborted. The entity <" << entity_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  StatusOr<ObjectAccessControl> result =
      client->CreateObjectAcl(bucket_name, object_name, entity_name, "OWNER");
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("OWNER", result->role());
  auto current_acl = client->ListObjectAcl(bucket_name, object_name);
  ASSERT_STATUS_OK(current_acl);
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result->entity(), *current_acl));

  auto get_result = client->GetObjectAcl(bucket_name, object_name, entity_name);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(*get_result, *result);

  ObjectAccessControl new_acl = *get_result;
  new_acl.set_role("READER");
  auto updated_result =
      client->UpdateObjectAcl(bucket_name, object_name, new_acl);
  ASSERT_STATUS_OK(updated_result);
  EXPECT_EQ("READER", updated_result->role());
  get_result = client->GetObjectAcl(bucket_name, object_name, entity_name);
  EXPECT_EQ(*get_result, *updated_result);

  new_acl = *get_result;
  new_acl.set_role("OWNER");
  // Because this is a freshly created object, with a random name, we do not
  // worry about implementing optimistic concurrency control.
  get_result = client->PatchObjectAcl(bucket_name, object_name, entity_name,
                                      *get_result, new_acl);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(get_result->role(), new_acl.role());

  // Remove an entity and verify it is no longer in the ACL.
  auto status = client->DeleteObjectAcl(bucket_name, object_name, entity_name);
  ASSERT_STATUS_OK(status);
  current_acl = client->ListObjectAcl(bucket_name, object_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_EQ(0, name_counter(result->entity(), *current_acl));

  status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, WriteWithContentType) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                                ContentType("text/plain"));
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, GetObjectMetadataFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto meta = client->GetObjectMetadata(bucket_name, object_name);
  EXPECT_FALSE(meta.ok()) << "value=" << meta.value();
}

TEST_F(ObjectIntegrationTest, StreamingWriteFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::badbit | std::ios_base::failbit);
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  testing_util::ExpectException<std::ios::failure>(
      [&] { os.Close(); },
      [&](std::ios::failure const&) {
        EXPECT_FALSE(os.metadata().ok());
        EXPECT_EQ(StatusCode::kFailedPrecondition,
                  os.metadata().status().code());
      },
      "" /* the message generated by the C++ runtime is unknown */);

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteFailureNoex) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client->InsertObject(
      bucket_name, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name, meta->bucket());

  auto os = client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().ok());
  EXPECT_EQ(StatusCode::kFailedPrecondition, os.metadata().status().code());

  client->DeleteObject(bucket_name, object_name);
}

TEST_F(ObjectIntegrationTest, ListObjectsFailure) {
  auto bucket_name = MakeRandomBucketName();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  ListObjectsReader reader = client->ListObjects(bucket_name, Versions(true));

  // This operation should fail because the bucket does not exist.
  TestPermanentFailure([&] {
    std::vector<ObjectMetadata> actual;
    for (auto&& o : reader) {
      actual.emplace_back(std::move(o).value());
    }
  });
}

TEST_F(ObjectIntegrationTest, DeleteObjectFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(ObjectIntegrationTest, UpdateObjectFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto update =
      client->UpdateObject(bucket_name, object_name, ObjectMetadata());
  EXPECT_FALSE(update.ok()) << "value=" << update.value();
}

TEST_F(ObjectIntegrationTest, PatchObjectFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto patch = client->PatchObject(bucket_name, object_name,
                                   ObjectMetadataPatchBuilder());
  EXPECT_FALSE(patch.ok()) << "value=" << patch.value();
}

TEST_F(ObjectIntegrationTest, ListAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto list = client->ListObjectAcl(bucket_name, object_name);
  ASSERT_FALSE(list.ok()) << "list[0]=" << list.value().front();
}

TEST_F(ObjectIntegrationTest, CreateAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl =
      client->CreateObjectAcl(bucket_name, object_name, entity_name, "READER");
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, GetAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client->GetObjectAcl(bucket_name, object_name, entity_name);
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, UpdateAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client->UpdateObjectAcl(
      bucket_name, object_name,
      ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, PatchAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client->PatchObjectAcl(
      bucket_name, object_name, entity_name, ObjectAccessControl(),
      ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, DeleteAccessControlFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto status = client->DeleteObjectAcl(bucket_name, object_name, entity_name);
  ASSERT_FALSE(status.ok());
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
              << " <project-id> <bucket-name>\n";
    return 1;
  }

  google::cloud::storage::flag_project_id = argv[1];
  google::cloud::storage::flag_bucket_name = argv[2];

  return RUN_ALL_TESTS();
}
