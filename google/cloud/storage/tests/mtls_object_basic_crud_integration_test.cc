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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;

using ::testing::Contains;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

struct MtlsObjectBasicCRUDIntegrationTest
    : public ::google::cloud::storage::testing::ObjectIntegrationTest {
 public:
  static StatusOr<std::string> ReadEnvVarFile(char const* env_var) {
    if (!GetEnv(env_var).has_value()) {
      return google::cloud::internal::InvalidArgumentError("Missing env var");
    }
    auto env = GetEnv(env_var);
    std::string key_file = std::move(*env);
    std::ifstream is(key_file);
    return std::string{std::istreambuf_iterator<char>{is}, {}};
  }

  static Client MakeMtlsClient(std::string const& test_key_file_contents,
                               std::string const& ssl_cert_blob,
                               std::string const& ssl_key_blob) {
    auto options = Options{}
                       .set<RetryPolicyOption>(TestRetryPolicy())
                       .set<BackoffPolicyOption>(TestBackoffPolicy());

    options.set<RestEndpointOption>("https://storage.mtls.googleapis.com");

    experimental::SslCertificate client_ssl_cert{ssl_cert_blob, ssl_key_blob};
    options.set<experimental::ClientSslCertificateOption>(client_ssl_cert);

    options.set<UnifiedCredentialsOption>(
        MakeServiceAccountCredentials(test_key_file_contents));

    return Client(std::move(options));
  }
};

/// @test Verify the Object CRUD (Create, Get, Update, Delete, List) operations.
TEST_F(MtlsObjectBasicCRUDIntegrationTest, BasicCRUD) {
  if (!(GetEnv("GOOGLE_CLOUD_CPP_CLIENT_SSL_CERT_FILE").has_value() &&
        GetEnv("GOOGLE_CLOUD_CPP_CLIENT_SSL_KEY_FILE").has_value())) {
    GTEST_SKIP();
  }

  ScopedEnvironment self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");
  auto test_key_file_contents =
      ReadEnvVarFile("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_STATUS_OK(test_key_file_contents);
  auto client_ssl_cert =
      ReadEnvVarFile("GOOGLE_CLOUD_CPP_CLIENT_SSL_CERT_FILE");
  ASSERT_STATUS_OK(client_ssl_cert);
  auto client_ssl_key = ReadEnvVarFile("GOOGLE_CLOUD_CPP_CLIENT_SSL_KEY_FILE");
  ASSERT_STATUS_OK(client_ssl_key);

  auto client = MakeMtlsClient(*test_key_file_contents, *client_ssl_cert,
                               *client_ssl_key);

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
