// Copyright 2021 Google LLC
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
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::MakeAccessTokenCredentials;
using ::google::cloud::internal::MakeGoogleDefaultCredentials;
using ::google::cloud::internal::UnifiedCredentialsOption;
using ::google::cloud::testing_util::IsOk;
using ::testing::StartsWith;

class UnifiedCredentialsIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  UnifiedCredentialsIntegrationTest()
      : grpc_config_("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", {}) {}

  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
  }

  static Client MakeTestClient(Options opts) {
    std::string const client_type = GetParam();
    // TODO(#...) - this should happen in CreateClient
    auto credentials = internal::MapCredentials(
        opts.get<google::cloud::internal::UnifiedCredentialsOption>());
    opts.set<internal::Oauth2CredentialsOption>(credentials);
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    if (client_type == "grpc") {
      return google::cloud::storage_experimental::DefaultGrpcClient(
                 std::move(opts))
          .value();
    }
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
    return internal::ClientImplDetails::CreateClient(std::move(credentials),
                                                     std::move(opts));
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
  testing_util::ScopedEnvironment grpc_config_;
};

void UseClient(Client client, std::string const& bucket_name,
               std::string const& object_name, std::string const& payload) {
  StatusOr<ObjectMetadata> meta = client.InsertObject(
      bucket_name, object_name, payload, IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  EXPECT_EQ(object_name, meta->name());

  auto stream = client.ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(payload, actual);

  auto status = client.DeleteObject(bucket_name, object_name);
  EXPECT_THAT(status, IsOk());
}

TEST_P(UnifiedCredentialsIntegrationTest, GoogleDefaultCredentials) {
  if (UsingEmulator()) GTEST_SKIP();
  auto client = MakeTestClient(
      Options{}.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials()));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

TEST_P(UnifiedCredentialsIntegrationTest, AccessToken) {
  if (UsingEmulator()) GTEST_SKIP();
  // First use the default credentials to obtain an access token, then use the
  // access token to test the DynamicAccessTokenCredentials() function. In a
  // real application one would fetch access tokens from something more
  // interesting, like the IAM credentials service. This is just a reasonably
  // easy way to get a working access token for the test.
  auto default_credentials = oauth2::GoogleDefaultCredentials();
  ASSERT_THAT(default_credentials, IsOk());
  auto expiration = std::chrono::system_clock::now() + std::chrono::hours(1);
  auto header = default_credentials.value()->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());

  auto constexpr kPrefix = "Authorization: Bearer ";
  ASSERT_THAT(*header, StartsWith(kPrefix));
  auto token = header->substr(std::strlen(kPrefix));

  auto client = MakeTestClient(Options{}.set<UnifiedCredentialsOption>(
      MakeAccessTokenCredentials(token, expiration)));

  ASSERT_NO_FATAL_FAILURE(
      UseClient(client, bucket_name(), MakeRandomObjectName(), LoremIpsum()));
}

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
INSTANTIATE_TEST_SUITE_P(UnifiedCredentialsGrpcIntegrationTest,
                         UnifiedCredentialsIntegrationTest,
                         ::testing::Values("grpc"));
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
INSTANTIATE_TEST_SUITE_P(UnifiedCredentialsRestIntegrationTest,
                         UnifiedCredentialsIntegrationTest,
                         ::testing::Values("rest"));

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
