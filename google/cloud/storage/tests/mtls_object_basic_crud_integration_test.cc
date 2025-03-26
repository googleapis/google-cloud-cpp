// Copyright 2025 Google LLC
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
#include "google/cloud/credentials.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/mtls_credentials_config.h"
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

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

struct MtlsObjectBasicCRUDIntegrationTest
    : public ::google::cloud::storage::testing::ObjectIntegrationTest {
 public:
  static Client MakeMtlsClient(std::string const& ssl_client_cert_filename,
                               std::string const& ssl_key_filename,
                               std::string const& ssl_key_file_password) {
    auto options = Options{}
                       .set<RetryPolicyOption>(TestRetryPolicy())
                       .set<BackoffPolicyOption>(TestBackoffPolicy());

    // Not sure if we need to set both Endpoint Options or not, but doing
    // it anyway for now.
    options.set<RestEndpointOption>("https://storage.mtls.googleapis.com:443");
    options.set<EndpointOption>("storage.mtls.googleapis.com:443");

    // Placeholder in case these need to be set.
    //    options.set<CARootsFilePathOption>();
    //    options.set<google::cloud::rest_internal::CAPathOption>();

    MtlsCredentialsConfig::Rest mtls_rest_config{
        ssl_client_cert_filename, ssl_key_filename, ssl_key_file_password};

    MtlsCredentialsConfig mtls_config;
    mtls_config.config = mtls_rest_config;
    options.set<UnifiedCredentialsOption>(
        MakeMtlsCredentials(ExperimentalTag{}, mtls_config, options));
    return Client(std::move(options));
  }
};

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(MtlsObjectBasicCRUDIntegrationTest, BasicCRUD) {
  // We'll need to leverage secret manager to plumb these env vars into the
  // test. If they aren't present skip this test for now.
  if (!(google::cloud::internal::GetEnv("SSL_CLIENT_CERT_FILENAME")
            .has_value() &&
        google::cloud::internal::GetEnv("SSL_KEY_FILENAME").has_value() &&
        google::cloud::internal::GetEnv("SSL_KEY_FILE_PASSWORD").has_value())) {
    GTEST_SKIP();
  }

  auto client = MakeMtlsClient(
      *google::cloud::internal::GetEnv("SSL_CLIENT_CERT_FILENAME"),
      *google::cloud::internal::GetEnv("SSL_KEY_FILENAME"),
      *google::cloud::internal::GetEnv("SSL_KEY_FILE_PASSWORD"));

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

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
