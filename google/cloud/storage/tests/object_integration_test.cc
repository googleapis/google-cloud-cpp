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

#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::AclEntityNames;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

using ObjectIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

TEST_F(ObjectIntegrationTest, FullPatch) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();
  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> original =
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(original);
  ScheduleForDelete(*original);

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
      client.PatchObject(bucket_name_, object_name, *original, desired);
  ASSERT_STATUS_OK(patched);

  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_THAT(AclEntityNames(patched->acl()),
              Contains("allAuthenticatedUsers").Times(1));

  EXPECT_EQ(desired.cache_control(), patched->cache_control());
  EXPECT_EQ(desired.content_disposition(), patched->content_disposition());
  EXPECT_EQ(desired.content_encoding(), patched->content_encoding());
  EXPECT_EQ(desired.content_language(), patched->content_language());
  EXPECT_EQ(desired.content_type(), patched->content_type());
  EXPECT_EQ(desired.metadata(), patched->metadata());
}

TEST_F(ObjectIntegrationTest, ListObjectsDelimiter) {
  auto client = MakeIntegrationTestClient();

  auto object_prefix = MakeRandomObjectName();
  for (auto const* suffix :
       {"/foo", "/foo/bar", "/foo/baz", "/qux/quux", "/something"}) {
    auto meta =
        client.InsertObject(bucket_name_, object_prefix + suffix, LoremIpsum(),
                            storage::IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  ListObjectsReader reader = client.ListObjects(
      bucket_name_, Prefix(object_prefix + "/"), Delimiter("/"));
  std::vector<std::string> actual;
  for (auto& it : reader) {
    auto const& meta = it.value();
    EXPECT_EQ(bucket_name_, meta.bucket());
    actual.push_back(meta.name());
  }
  EXPECT_THAT(actual, UnorderedElementsAre(object_prefix + "/foo",
                                           object_prefix + "/something"));
  reader = client.ListObjects(bucket_name_, Prefix(object_prefix));
}

TEST_F(ObjectIntegrationTest, ListObjectsAndPrefixes) {
  auto client = MakeIntegrationTestClient();

  auto object_prefix = MakeRandomObjectName();
  for (auto const* suffix :
       {"/foo", "/foo/bar", "/foo/baz", "/qux/quux", "/something"}) {
    auto meta =
        client.InsertObject(bucket_name_, object_prefix + suffix, LoremIpsum(),
                            storage::IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  ListObjectsAndPrefixesReader reader = client.ListObjectsAndPrefixes(
      bucket_name_, Prefix(object_prefix + "/"), Delimiter("/"));
  std::vector<std::string> prefixes;
  std::vector<std::string> objects;
  for (auto& it : reader) {
    auto const& result = it.value();
    if (absl::holds_alternative<std::string>(result)) {
      prefixes.push_back(absl::get<std::string>(result));
    } else {
      auto const& meta = absl::get<ObjectMetadata>(result);
      EXPECT_EQ(bucket_name_, meta.bucket());
      objects.push_back(meta.name());
    }
  }
  EXPECT_THAT(prefixes, UnorderedElementsAre(object_prefix + "/qux/",
                                             object_prefix + "/foo/"));
  EXPECT_THAT(objects, UnorderedElementsAre(object_prefix + "/something",
                                            object_prefix + "/foo"));
}

TEST_F(ObjectIntegrationTest, ListObjectsAndPrefixesWithFolders) {
  auto client = MakeIntegrationTestClient();

  auto object_prefix = MakeRandomObjectName();
  for (auto const* suffix :
       {"/foo", "/foo/bar", "/foo/baz", "/qux/quux", "/something"}) {
    auto meta =
        client.InsertObject(bucket_name_, object_prefix + suffix, LoremIpsum(),
                            storage::IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  auto reader = client.ListObjectsAndPrefixes(
      bucket_name_, IncludeFoldersAsPrefixes(true), Prefix(object_prefix + "/"),
      Delimiter("/"));
  std::vector<std::string> prefixes;
  std::vector<std::string> objects;
  for (auto& it : reader) {
    auto const& result = it.value();
    if (absl::holds_alternative<std::string>(result)) {
      prefixes.push_back(absl::get<std::string>(result));
    } else {
      auto const& meta = absl::get<ObjectMetadata>(result);
      EXPECT_EQ(bucket_name_, meta.bucket());
      objects.push_back(meta.name());
    }
  }
  EXPECT_THAT(prefixes, UnorderedElementsAre(object_prefix + "/qux/",
                                             object_prefix + "/foo/"));
  EXPECT_THAT(objects, UnorderedElementsAre(object_prefix + "/something",
                                            object_prefix + "/foo"));
}

TEST_F(ObjectIntegrationTest, ListObjectsStartEndOffset) {
  auto client = MakeIntegrationTestClient();

  auto object_prefix = MakeRandomObjectName();
  for (auto const* suffix :
       {"/foo", "/foo/bar", "/foo/baz", "/qux/quux", "/something"}) {
    auto meta =
        client.InsertObject(bucket_name_, object_prefix + suffix, LoremIpsum(),
                            storage::IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  ListObjectsAndPrefixesReader reader = client.ListObjectsAndPrefixes(
      bucket_name_, Prefix(object_prefix + "/"), Delimiter("/"),
      StartOffset(object_prefix + "/foo"), EndOffset(object_prefix + "/qux"));
  std::vector<std::string> prefixes;
  std::vector<std::string> objects;
  for (auto& it : reader) {
    auto const& result = it.value();
    if (absl::holds_alternative<std::string>(result)) {
      prefixes.push_back(absl::get<std::string>(result));
    } else {
      auto const& meta = absl::get<ObjectMetadata>(result);
      EXPECT_EQ(bucket_name_, meta.bucket());
      objects.push_back(meta.name());
    }
  }
  EXPECT_THAT(prefixes, UnorderedElementsAre(object_prefix + "/foo/"));
  EXPECT_THAT(objects, UnorderedElementsAre(object_prefix + "/foo"));
}

TEST_F(ObjectIntegrationTest, ListObjectsMatchGlob) {
  auto client = MakeIntegrationTestClient();

  auto object_prefix = MakeRandomObjectName();
  for (auto const* suffix :
       {"/foo/1.txt", "/foo/bar/1.txt", "/foo/bar/2.cc", "/qux/quux/3.cc"}) {
    auto meta =
        client.InsertObject(bucket_name_, object_prefix + suffix, LoremIpsum(),
                            storage::IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  auto reader = client.ListObjects(bucket_name_, Prefix(object_prefix),
                                   MatchGlob("**/*.cc"));
  std::vector<std::string> objects;
  for (auto& o : reader) {
    ASSERT_STATUS_OK(o);
    objects.push_back(o->name());
  }
  EXPECT_THAT(objects, UnorderedElementsAre(object_prefix + "/foo/bar/2.cc",
                                            object_prefix + "/qux/quux/3.cc"));
}

TEST_F(ObjectIntegrationTest, ListObjectsIncludeTrailingDelimiter) {
  auto client = MakeIntegrationTestClient();

  auto object_prefix = MakeRandomObjectName();
  for (auto const* suffix : {"/foo", "/foo/", "/foo/bar", "/foo/baz", "/qux/",
                             "/qux/quux", "/something", "/something/"}) {
    auto meta =
        client.InsertObject(bucket_name_, object_prefix + suffix, LoremIpsum(),
                            storage::IfGenerationMatch(0));
    ASSERT_STATUS_OK(meta);
    ScheduleForDelete(*meta);
  }

  ListObjectsAndPrefixesReader reader = client.ListObjectsAndPrefixes(
      bucket_name_, Prefix(object_prefix + "/"), Delimiter("/"),
      IncludeTrailingDelimiter(true));
  std::vector<std::string> prefixes;
  std::vector<std::string> objects;
  for (auto& it : reader) {
    auto const& result = it.value();
    if (absl::holds_alternative<std::string>(result)) {
      prefixes.push_back(absl::get<std::string>(result));
    } else {
      auto const& meta = absl::get<ObjectMetadata>(result);
      EXPECT_EQ(bucket_name_, meta.bucket());
      objects.push_back(meta.name());
    }
  }
  EXPECT_THAT(prefixes, UnorderedElementsAre(object_prefix + "/foo/",
                                             object_prefix + "/something/",
                                             object_prefix + "/qux/"));
  EXPECT_THAT(objects, UnorderedElementsAre(object_prefix + "/foo",
                                            object_prefix + "/foo/",
                                            object_prefix + "/something",
                                            object_prefix + "/something/",
                                            object_prefix + "/qux/"));
}

TEST_F(ObjectIntegrationTest, BasicReadWrite) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectIntegrationTest, BasicReadWriteBinary) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();
  auto const expected = [] {
    std::string result;
    auto constexpr kPayloadSize = 2 * 1024;
    auto const min = std::numeric_limits<char>::min();
    auto const max = std::numeric_limits<char>::max();
    for (char i = min; i != max; ++i) {
      result.push_back(i);
      if (result.size() >= kPayloadSize) return result;
    }
    result.append(std::string(kPayloadSize - result.size(), '\0'));
    return result;
  }();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectIntegrationTest, EncryptedReadWrite) {
  // TODO(#14385) - the emulator does not support this feature for gRPC.
  if (UsingEmulator() && UsingGrpc()) GTEST_SKIP();

  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();
  EncryptionKeyData key = MakeEncryptionKeyData();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client.InsertObject(bucket_name_, object_name, expected,
                          IfGenerationMatch(0), EncryptionKey(key));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  ASSERT_TRUE(meta->has_customer_encryption());
  EXPECT_EQ("AES256", meta->customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta->customer_encryption().key_sha256);

  // Create a iostream to read the object back.
  auto stream =
      client.ReadObject(bucket_name_, object_name, EncryptionKey(key));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectIntegrationTest, ReadNotFound) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  EXPECT_FALSE(stream.status().ok());
  EXPECT_FALSE(stream.IsOpen());
  EXPECT_EQ(StatusCode::kNotFound, stream.status().code())
      << "status=" << stream.status();
  EXPECT_TRUE(stream.bad());
}

TEST_F(ObjectIntegrationTest, StreamingWrite) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);
}

TEST_F(ObjectIntegrationTest, StreamingResumableWriteSizeMismatch) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already. Expect its length
  // to be 3 bytes.
  auto os =
      client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                         NewResumableUploadSession(), UploadContentLength(3));

  // Write much more than 3 bytes.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  auto meta = os.metadata();
  if (!UsingGrpc()) {
    EXPECT_THAT(meta, Not(IsOk())) << "value=" << meta.value();
    EXPECT_THAT(meta, StatusIs(StatusCode::kInvalidArgument));
  }
}

TEST_F(ObjectIntegrationTest, StreamingWriteAutoClose) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::string expected = "A short string to test\n";

  {
    // Create the object, but only if it does not exist already.
    auto os =
        client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
    os << expected;
  }
  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected, actual);

  auto status = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteEmpty) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);
  ASSERT_EQ(object_name, meta.name());
  ASSERT_EQ(bucket_name_, meta.bucket());
  ASSERT_EQ(0U, meta.size());

  // Create a iostream to read the object back.
  auto stream = client.ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_TRUE(actual.empty());
}

TEST_F(ObjectIntegrationTest, AccessControlCRUD) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto insert = client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                                    IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  auto entity_name = MakeEntityName();
  StatusOr<std::vector<ObjectAccessControl>> initial_acl =
      client.ListObjectAcl(bucket_name_, object_name);
  ASSERT_STATUS_OK(initial_acl);
  ASSERT_THAT(AclEntityNames(*initial_acl), Not(Contains(entity_name)))
      << "Test aborted. The entity <" << entity_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  StatusOr<ObjectAccessControl> result =
      client.CreateObjectAcl(bucket_name_, object_name, entity_name, "OWNER");
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("OWNER", result->role());
  auto current_acl = client.ListObjectAcl(bucket_name_, object_name);
  ASSERT_STATUS_OK(current_acl);
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_THAT(AclEntityNames(*current_acl),
              Contains(result->entity()).Times(1));

  auto get_result = client.GetObjectAcl(bucket_name_, object_name, entity_name);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(*get_result, *result);

  ObjectAccessControl new_acl = *get_result;
  new_acl.set_role("READER");
  auto updated_result =
      client.UpdateObjectAcl(bucket_name_, object_name, new_acl);
  ASSERT_STATUS_OK(updated_result);
  EXPECT_EQ("READER", updated_result->role());
  get_result = client.GetObjectAcl(bucket_name_, object_name, entity_name);
  EXPECT_EQ(*get_result, *updated_result);

  new_acl = *get_result;
  new_acl.set_role("OWNER");
  // Because this is a freshly created object, with a random name, we do not
  // worry about implementing optimistic concurrency control.
  get_result = client.PatchObjectAcl(bucket_name_, object_name, entity_name,
                                     *get_result, new_acl);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(get_result->role(), new_acl.role());

  // Remove an entity and verify it is no longer in the ACL.
  auto status = client.DeleteObjectAcl(bucket_name_, object_name, entity_name);
  ASSERT_STATUS_OK(status);
  current_acl = client.ListObjectAcl(bucket_name_, object_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl), Not(Contains(result->entity())));
}

TEST_F(ObjectIntegrationTest, WriteWithContentType) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               ContentType("text/plain"));
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  os.Close();
  ObjectMetadata meta = os.metadata().value();
  ScheduleForDelete(meta);

  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name_, meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());
}

TEST_F(ObjectIntegrationTest, GetObjectMetadataFailure) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto meta = client.GetObjectMetadata(bucket_name_, object_name);
  EXPECT_THAT(meta, Not(IsOk())) << "value=" << meta.value();
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST_F(ObjectIntegrationTest, StreamingWriteFailure) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::badbit | std::ios_base::failbit);
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  ASSERT_THROW(os.Close(), std::ios::failure);
  // The GCS server returns a different error code depending on the
  // protocol (REST vs. gRPC) used
  EXPECT_THAT(os.metadata(), AnyOf(StatusIs(StatusCode::kFailedPrecondition),
                                   StatusIs(StatusCode::kAborted)));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST_F(ObjectIntegrationTest, StreamingWriteFailureNoex) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());

  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0));
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_TRUE(os.bad());
  // The GCS server returns a different error code depending on the
  // protocol (REST vs. gRPC) used
  EXPECT_THAT(
      os.metadata().status().code(),
      AnyOf(Eq(StatusCode::kFailedPrecondition), Eq(StatusCode::kAborted)))
      << " status=" << os.metadata().status();
}

TEST_F(ObjectIntegrationTest, ListObjectsFailure) {
  auto bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient();

  // This operation should fail because the bucket does not exist.
  ListObjectsReader reader = client.ListObjects(bucket_name, Versions(true));
  auto it = reader.begin();
  ASSERT_TRUE(it != reader.end());
  EXPECT_THAT(*it, Not(IsOk()));
}

TEST_F(ObjectIntegrationTest, DeleteObjectFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto status = client.DeleteObject(bucket_name_, object_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(ObjectIntegrationTest, UpdateObjectFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto update =
      client.UpdateObject(bucket_name_, object_name, ObjectMetadata());
  EXPECT_THAT(update, Not(IsOk())) << "value=" << update.value();
}

TEST_F(ObjectIntegrationTest, PatchObjectFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto patch = client.PatchObject(bucket_name_, object_name,
                                  ObjectMetadataPatchBuilder());
  EXPECT_THAT(patch, Not(IsOk())) << "value=" << patch.value();
}

TEST_F(ObjectIntegrationTest, ListAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto list = client.ListObjectAcl(bucket_name_, object_name);
  ASSERT_FALSE(list.ok()) << "list[0]=" << list.value().front();
}

TEST_F(ObjectIntegrationTest, CreateAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl =
      client.CreateObjectAcl(bucket_name_, object_name, entity_name, "READER");
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, GetAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client.GetObjectAcl(bucket_name_, object_name, entity_name);
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, UpdateAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client.UpdateObjectAcl(
      bucket_name_, object_name,
      ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, PatchAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client.PatchObjectAcl(
      bucket_name_, object_name, entity_name, ObjectAccessControl(),
      ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, DeleteAccessControlFailure) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto status = client.DeleteObjectAcl(bucket_name_, object_name, entity_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(ObjectIntegrationTest, DeleteResumableUpload) {
  auto client = MakeIntegrationTestClient(Options{}.set<RetryPolicyOption>(
      LimitedErrorCountRetryPolicy(1).clone()));

  auto object_name = MakeRandomObjectName();
  auto stream = client.WriteObject(bucket_name_, object_name,
                                   NewResumableUploadSession());
  auto session_id = stream.resumable_session_id();

  stream << "This data will not get uploaded, it is too small\n";
  std::move(stream).Suspend();

  auto status = client.DeleteResumableUpload(session_id);
  EXPECT_STATUS_OK(status);

  Client client_resumable(Options{}.set<MaximumSimpleUploadSizeOption>(0));
  auto stream_resumable = client_resumable.WriteObject(
      bucket_name_, object_name, RestoreResumableUploadSession(session_id));
  stream_resumable << LoremIpsum();
  stream_resumable.Close();
  EXPECT_FALSE(stream_resumable.metadata().status().ok());
}

TEST_F(ObjectIntegrationTest, InsertWithCustomTime) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  using std::chrono::seconds;
  auto const custom_time = std::chrono::system_clock::now() + seconds(5);
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name_, object_name, expected, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata{}.set_custom_time(custom_time)));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  EXPECT_EQ(custom_time, meta->custom_time());
}

TEST_F(ObjectIntegrationTest, WriteWithCustomTime) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();
  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  using std::chrono::seconds;
  auto const custom_time = std::chrono::system_clock::now() + seconds(5);
  auto stream = client.WriteObject(
      bucket_name_, object_name, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata{}.set_custom_time(custom_time)));
  stream << expected;
  stream.Close();
  auto meta = stream.metadata();
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name_, meta->bucket());
  EXPECT_EQ(custom_time, meta->custom_time());
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
