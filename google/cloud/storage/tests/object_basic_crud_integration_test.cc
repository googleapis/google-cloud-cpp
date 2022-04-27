// Copyright 2020 Google LLC
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
#include "google/cloud/storage/testing/object_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ContainsOnce;
using ::testing::Contains;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;
using ObjectBasicCRUDIntegrationTest =
    ::google::cloud::storage::testing::ObjectIntegrationTest;

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(ObjectBasicCRUDIntegrationTest, BasicCRUD) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto list_object_names = [&client, this] {
    std::vector<std::string> names;
    for (auto o : client->ListObjects(bucket_name_)) {
      EXPECT_STATUS_OK(o);
      if (!o) break;
      names.push_back(o->name());
    }
    return names;
  };

  auto object_name = MakeRandomObjectName();
  ASSERT_THAT(list_object_names(), Not(Contains(object_name)))
      << "Test aborted. The object <" << object_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert_meta =
      client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                           IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_THAT(list_object_names(), ContainsOnce(object_name));

  StatusOr<ObjectMetadata> get_meta = client->GetObjectMetadata(
      bucket_name_, object_name, Generation(insert_meta->generation()),
      Projection("full"));
  ASSERT_STATUS_OK(get_meta);

  if (UsingGrpc()) {
    // The metadata returned by gRPC (InsertObject) doesn't contain the `acl`,
    // `etag`, `media_link`, or `self_link` fields. Just compare field by field:
    EXPECT_EQ(get_meta->name(), insert_meta->name());
    // EXPECT_EQ(get_meta->acl(), insert_meta->acl());
    EXPECT_EQ(get_meta->bucket(), insert_meta->bucket());
    EXPECT_EQ(get_meta->cache_control(), insert_meta->cache_control());
    EXPECT_EQ(get_meta->component_count(), insert_meta->component_count());
    EXPECT_EQ(get_meta->content_disposition(),
              insert_meta->content_disposition());
    EXPECT_EQ(get_meta->content_encoding(), insert_meta->content_encoding());
    EXPECT_EQ(get_meta->content_type(), insert_meta->content_type());
    EXPECT_EQ(get_meta->crc32c(), insert_meta->crc32c());
    EXPECT_EQ(get_meta->event_based_hold(), insert_meta->event_based_hold());
    EXPECT_EQ(get_meta->generation(), insert_meta->generation());
    EXPECT_EQ(get_meta->id(), insert_meta->id());
    EXPECT_EQ(get_meta->kind(), insert_meta->kind());
    EXPECT_EQ(get_meta->kms_key_name(), insert_meta->kms_key_name());
    EXPECT_EQ(get_meta->md5_hash(), insert_meta->md5_hash());
    EXPECT_EQ(get_meta->media_link(), insert_meta->media_link());
    EXPECT_EQ(get_meta->metageneration(), insert_meta->metageneration());
    // EXPECT_EQ(get_meta->owner(), insert_meta->owner());
    EXPECT_EQ(get_meta->retention_expiration_time(),
              insert_meta->retention_expiration_time());
    EXPECT_EQ(get_meta->self_link(), insert_meta->self_link());
    EXPECT_EQ(get_meta->size(), insert_meta->size());
    EXPECT_EQ(get_meta->storage_class(), insert_meta->storage_class());
    EXPECT_EQ(get_meta->temporary_hold(), insert_meta->temporary_hold());
    EXPECT_EQ(get_meta->time_created(), insert_meta->time_created());
    EXPECT_EQ(get_meta->time_deleted(), insert_meta->time_deleted());
    EXPECT_EQ(get_meta->time_storage_class_updated(),
              insert_meta->time_storage_class_updated());
    EXPECT_EQ(get_meta->updated(), insert_meta->updated());
  } else {
    EXPECT_EQ(*get_meta, *insert_meta);
  }

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
      bucket_name_, object_name, update, Projection("full"));
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
    EXPECT_THAT(expected, UnorderedElementsAreArray(actual));
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
  StatusOr<ObjectMetadata> patched_meta =
      client->PatchObject(bucket_name_, object_name, *updated_meta,
                          desired_patch, PredefinedAcl::Private());
  ASSERT_STATUS_OK(patched_meta);

  EXPECT_EQ(desired_patch.metadata(), patched_meta->metadata())
      << *patched_meta;
  EXPECT_EQ(desired_patch.content_language(), patched_meta->content_language())
      << *patched_meta;

  // This is the test for Object CRUD, we cannot rely on `ScheduleForDelete()`.
  auto status = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(list_object_names(), Not(Contains(object_name)));
}

Client CreateNonDefaultClient() {
  auto emulator =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
  google::cloud::testing_util::ScopedEnvironment env(
      "CLOUD_STORAGE_EMULATOR_ENDPOINT", {});
  auto options = google::cloud::Options{};
  if (!emulator) {
    // Use a different spelling of the default endpoint. This disables the
    // allegedly "slightly faster" XML endpoints, but should continue to work.
    options.set<RestEndpointOption>("https://storage.googleapis.com:443");
    options.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials());
  } else {
    // Use the emulator endpoint, but not through the environment variable
    options.set<RestEndpointOption>(*emulator);
    options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  }
  return Client(std::move(options));
}

/// @test Verify that the client works with non-default endpoints.
TEST_F(ObjectBasicCRUDIntegrationTest, NonDefaultEndpointInsertJSON) {
  auto client = CreateNonDefaultClient();
  auto object_name = MakeRandomObjectName();
  auto const expected = LoremIpsum();
  auto insert = client.InsertObject(bucket_name_, object_name, expected);
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  auto stream =
      client.ReadObject(bucket_name_, object_name, IfGenerationNotMatch(0));
  EXPECT_STATUS_OK(stream.status());
  std::string const actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

/// @test Verify that the client works with non-default endpoints.
TEST_F(ObjectBasicCRUDIntegrationTest, NonDefaultEndpointInsertXml) {
  auto client = CreateNonDefaultClient();
  auto object_name = MakeRandomObjectName();
  auto const expected = LoremIpsum();
  auto insert =
      client.InsertObject(bucket_name_, object_name, expected, Fields(""));
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  auto stream = client.ReadObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(stream.status());
  std::string const actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

/// @test Verify that the client works with non-default endpoints.
TEST_F(ObjectBasicCRUDIntegrationTest, NonDefaultEndpointWriteJSON) {
  auto client = CreateNonDefaultClient();
  auto object_name = MakeRandomObjectName();
  auto const expected = LoremIpsum();
  auto writer = client.WriteObject(bucket_name_, object_name);
  writer << expected;
  writer.Close();
  ASSERT_STATUS_OK(writer.metadata());
  ScheduleForDelete(*writer.metadata());
  auto stream =
      client.ReadObject(bucket_name_, object_name, IfGenerationNotMatch(0));
  EXPECT_STATUS_OK(stream.status());
  std::string const actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

/// @test Verify that the client works with non-default endpoints.
TEST_F(ObjectBasicCRUDIntegrationTest, NonDefaultEndpointWriteXml) {
  auto client = CreateNonDefaultClient();
  auto object_name = MakeRandomObjectName();
  auto const expected = LoremIpsum();
  auto writer = client.WriteObject(bucket_name_, object_name, Fields(""));
  writer << expected;
  writer.Close();
  ASSERT_STATUS_OK(writer.metadata());
  ScheduleForDelete(*writer.metadata());
  auto stream = client.ReadObject(bucket_name_, object_name);
  EXPECT_STATUS_OK(stream.status());
  std::string const actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);
}

/// @test Verify inserting an object does not set the customTime attribute.
TEST_F(ObjectBasicCRUDIntegrationTest, InsertWithoutCustomTime) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto insert = client->InsertObject(bucket_name_, object_name, LoremIpsum(),
                                     IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(insert);
  EXPECT_FALSE(insert->has_custom_time());

  auto get = client->GetObjectMetadata(bucket_name_, object_name);
  ASSERT_STATUS_OK(get);
  EXPECT_FALSE(get->has_custom_time());

  auto patch = client->PatchObject(
      bucket_name_, object_name,
      ObjectMetadataPatchBuilder().SetContentType("text/plain"));
  ASSERT_STATUS_OK(patch);
  EXPECT_FALSE(patch->has_custom_time());

  get = client->GetObjectMetadata(bucket_name_, object_name);
  ASSERT_STATUS_OK(get);
  EXPECT_FALSE(get->has_custom_time());

  auto status = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify writing an object does not set the customTime attribute.
TEST_F(ObjectBasicCRUDIntegrationTest, WriteWithoutCustomTime) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();
  auto os = client->WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                                Projection("full"));
  os << LoremIpsum();
  os.Close();
  auto metadata = os.metadata();
  ASSERT_STATUS_OK(metadata);
  EXPECT_FALSE(metadata->has_custom_time());

  auto get = client->GetObjectMetadata(bucket_name_, object_name);
  ASSERT_STATUS_OK(get);
  EXPECT_FALSE(get->has_custom_time());

  auto status = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
