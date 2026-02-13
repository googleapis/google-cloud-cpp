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
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace {
// Helper function to check if a time point is set (i.e. not the default value).
bool IsSet(std::chrono::system_clock::time_point tp) {
  return tp != std::chrono::system_clock::time_point{};
}
}  // namespace

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

struct ObjectBasicCRUDIntegrationTest
    : public ::google::cloud::storage::testing::ObjectIntegrationTest {
 public:
  static Client MakeNonDefaultClient() {
    // Use a different spelling of the default endpoint.
    auto options = MakeTestOptions();
    auto endpoint = options.get<RestEndpointOption>();
    if (absl::StartsWith(endpoint, "https") &&
        !absl::EndsWith(endpoint, ":443")) {
      options.set<RestEndpointOption>(endpoint + ":443");
    }
    endpoint = options.get<EndpointOption>();
    if (endpoint.empty()) {
      options.set<EndpointOption>("storage.googleapis.com:443");
    } else if (!absl::EndsWith(endpoint, ":443")) {
      options.set<EndpointOption>(endpoint + ":443");
    }
    return MakeIntegrationTestClient(std::move(options));
  }

  void SetUp() override {
    // 1. Run the base class SetUp first. This initializes 'bucket_name_'
    //    from the environment variable.
    ::google::cloud::storage::testing::ObjectIntegrationTest::SetUp();

    // 2. Create a client to interact with the emulator/backend.
    auto client = MakeIntegrationTestClient();

    // 3. Check if the bucket exists.
    auto metadata = client.GetBucketMetadata(bucket_name_);

    // 4. If it's missing (kNotFound), create it.
    if (metadata.status().code() == StatusCode::kNotFound) {
      // Use a default project ID if the env var isn't set (common in local
      // emulators).
      auto project_id = google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT")
                            .value_or("test-project");

      auto created = client.CreateBucketForProject(bucket_name_, project_id,
                                                   BucketMetadata());
      ASSERT_STATUS_OK(created)
          << "Failed to auto-create missing bucket: " << bucket_name_;
    } else {
      // If it exists (or failed for another reason), assert it is OK.
      ASSERT_STATUS_OK(metadata)
          << "Failed to verify bucket existence: " << bucket_name_;
    }
  }
};

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(ObjectBasicCRUDIntegrationTest, BasicCRUD) {
  auto client = MakeIntegrationTestClient();

  auto list_object_names = [&client, this] {
    std::vector<std::string> names;
    for (auto o : client.ListObjects(bucket_name_)) {
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
      client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                          IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_THAT(list_object_names(), Contains(object_name).Times(1));

  StatusOr<ObjectMetadata> get_meta = client.GetObjectMetadata(
      bucket_name_, object_name, Generation(insert_meta->generation()),
      Projection("full"));
  ASSERT_STATUS_OK(get_meta);
  EXPECT_EQ(*get_meta, *insert_meta);
  EXPECT_FALSE(insert_meta->has_contexts()) << *insert_meta;

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
  StatusOr<ObjectMetadata> updated_meta = client.UpdateObject(
      bucket_name_, object_name, update, Projection("full"));
  ASSERT_STATUS_OK(updated_meta);

  // Because some ACL field values are not predictable, we convert the values we
  // care about to strings and compare those.
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
      client.PatchObject(bucket_name_, object_name, *updated_meta,
                         desired_patch, PredefinedAcl::Private());
  ASSERT_STATUS_OK(patched_meta);

  EXPECT_EQ(desired_patch.metadata(), patched_meta->metadata())
      << *patched_meta;
  EXPECT_EQ(desired_patch.content_language(), patched_meta->content_language())
      << *patched_meta;

  // This is the test for Object CRUD, we cannot rely on `ScheduleForDelete()`.
  auto status = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(list_object_names(), Not(Contains(object_name)));
}

/// @test Verify the Object CRUD operations with object contexts.
TEST_F(ObjectBasicCRUDIntegrationTest, BasicCRUDWithObjectContexts) {
  auto client = MakeIntegrationTestClient();

  auto list_object_names = [&client, this] {
    std::vector<std::string> names;
    for (auto o : client.ListObjects(bucket_name_)) {
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

  // Create the object, but only if it does not exist already, inserting
  // a custom context {"department": "engineering"}.
  ObjectContexts contexts;
  ObjectCustomContextPayload payload;
  payload.value = "engineering";
  contexts.upsert_custom_context("department", std::move(payload));
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name_, object_name, LoremIpsum(), IfGenerationMatch(0),
      Projection("full"),
      WithObjectMetadata(ObjectMetadata().set_contexts(contexts)));
  ASSERT_STATUS_OK(insert_meta);
  EXPECT_THAT(list_object_names(), Contains(object_name).Times(1));

  // Verify the response ObjectMetadata has the custom contexts we set.
  StatusOr<ObjectMetadata> get_meta = client.GetObjectMetadata(
      bucket_name_, object_name, Generation(insert_meta->generation()),
      Projection("full"));
  ASSERT_STATUS_OK(get_meta);
  EXPECT_TRUE(get_meta->has_contexts()) << *get_meta;
  EXPECT_TRUE(get_meta->contexts().has_custom("department")) << *get_meta;
  EXPECT_EQ("engineering",
            get_meta->contexts().custom().at("department")->value)
      << *get_meta;
  EXPECT_TRUE(
      IsSet(get_meta->contexts().custom().at("department")->update_time))
      << *get_meta;
  EXPECT_TRUE(
      IsSet(get_meta->contexts().custom().at("department")->create_time))
      << *get_meta;

  // Update object with a new value "engineering and research" for the existing
  // custom context, and also add another custom context {"region":
  // "Asia Pacific"}.
  ObjectMetadata update = *get_meta;
  ObjectContexts contexts_updated;
  ObjectCustomContextPayload payload_updated;
  payload_updated.value = "engineering and research";
  contexts_updated.upsert_custom_context("department",
                                         std::move(payload_updated));
  ObjectCustomContextPayload payload_another;
  payload_another.value = "Asia Pacific";
  contexts_updated.upsert_custom_context("region", std::move(payload_another));
  update.set_contexts(contexts_updated);
  StatusOr<ObjectMetadata> updated_meta = client.UpdateObject(
      bucket_name_, object_name, update, Projection("full"));
  ASSERT_STATUS_OK(updated_meta);

  // Verify the response ObjectMetadata has the updated custom contexts.
  EXPECT_TRUE(updated_meta->has_contexts()) << *updated_meta;
  EXPECT_TRUE(updated_meta->contexts().has_custom("department"))
      << *updated_meta;
  // EXPECT_EQ("engineering and research",
  //           updated_meta->contexts().custom().at("department")->value)
  //     << *updated_meta;
  EXPECT_TRUE(updated_meta->contexts().has_custom("region")) << *updated_meta;
  // EXPECT_EQ("Asia Pacific",
  //           updated_meta->contexts().custom().at("region")->value)
  //     << *updated_meta;
  EXPECT_TRUE(
      IsSet(updated_meta->contexts().custom().at("region")->update_time))
      << *updated_meta;
  EXPECT_TRUE(
      IsSet(updated_meta->contexts().custom().at("region")->create_time))
      << *updated_meta;

  // Update object with deletion of the "department" custom context.
  ObjectContexts contexts_deleted;
  contexts_deleted.upsert_custom_context("department", absl::nullopt);
  update.set_contexts(contexts_deleted);
  StatusOr<ObjectMetadata> deleted_meta = client.UpdateObject(
      bucket_name_, object_name, update, Projection("full"));
  ASSERT_STATUS_OK(deleted_meta);

  // Verify the response ObjectMetadata has the "department" key is set to null,
  // but the "region" key still present with value.
  EXPECT_TRUE(deleted_meta->has_contexts()) << *deleted_meta;
  EXPECT_FALSE(deleted_meta->contexts().has_custom("department"))
      << *deleted_meta;
  EXPECT_TRUE(deleted_meta->contexts().has_custom("region")) << *deleted_meta;
  // EXPECT_TRUE(deleted_meta->contexts().custom().at("region").has_value())
  //     << *deleted_meta;
  // EXPECT_EQ("Asia Pacific",
  //           deleted_meta->contexts().custom().at("region")->value)
  //     << *deleted_meta;

  // Update object with reset of the custom field.
  ObjectContexts contexts_reset;
  update.set_contexts(contexts_reset);
  StatusOr<ObjectMetadata> reset_meta = client.UpdateObject(
      bucket_name_, object_name, update, Projection("full"));
  ASSERT_STATUS_OK(reset_meta);

  // Verify the response ObjectMetadata that no custom contexts exists. This
  // is the default behavior as if the custom field is never set.
  EXPECT_FALSE(reset_meta->has_contexts()) << *reset_meta;

  // This is the test for Object CRUD, we cannot rely on `ScheduleForDelete()`.
  auto status = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(list_object_names(), Not(Contains(object_name)));
}

/// @test Verify that the client works with non-default endpoints.
TEST_F(ObjectBasicCRUDIntegrationTest, NonDefaultEndpointInsert) {
  auto client = MakeNonDefaultClient();
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
TEST_F(ObjectBasicCRUDIntegrationTest, NonDefaultEndpointWrite) {
  auto client = MakeNonDefaultClient();
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

/// @test Verify inserting an object does not set the customTime attribute.
TEST_F(ObjectBasicCRUDIntegrationTest, InsertWithoutCustomTime) {
  auto client = MakeIntegrationTestClient();
  auto object_name = MakeRandomObjectName();

  auto insert = client.InsertObject(bucket_name_, object_name, LoremIpsum(),
                                    IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(insert);
  EXPECT_FALSE(insert->has_custom_time());

  auto get = client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_STATUS_OK(get);
  EXPECT_FALSE(get->has_custom_time());

  auto patch = client.PatchObject(
      bucket_name_, object_name,
      ObjectMetadataPatchBuilder().SetContentType("text/plain"));
  ASSERT_STATUS_OK(patch);
  EXPECT_FALSE(patch->has_custom_time());

  get = client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_STATUS_OK(get);
  EXPECT_FALSE(get->has_custom_time());

  auto status = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify writing an object does not set the customTime attribute.
TEST_F(ObjectBasicCRUDIntegrationTest, WriteWithoutCustomTime) {
  auto client = MakeIntegrationTestClient();

  auto object_name = MakeRandomObjectName();
  auto os = client.WriteObject(bucket_name_, object_name, IfGenerationMatch(0),
                               Projection("full"));
  os << LoremIpsum();
  os.Close();
  auto metadata = os.metadata();
  ASSERT_STATUS_OK(metadata);
  EXPECT_FALSE(metadata->has_custom_time());

  auto get = client.GetObjectMetadata(bucket_name_, object_name);
  ASSERT_STATUS_OK(get);
  EXPECT_FALSE(get->has_custom_time());

  auto status = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(status);
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
