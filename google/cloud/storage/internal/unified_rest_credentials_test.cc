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

#include "google/cloud/storage/internal/unified_rest_credentials.h"
#include "google/cloud/storage/testing/constants.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <random>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::MakeAccessTokenCredentials;
using ::google::cloud::MakeGoogleDefaultCredentials;
using ::google::cloud::MakeInsecureCredentials;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsEmpty;

class UnifiedRestCredentialsTest : public ::testing::Test {
 public:
  UnifiedRestCredentialsTest() : generator_(std::random_device{}()) {}

  std::string TempKeyFileName() {
    return ::testing::TempDir() +
           ::google::cloud::internal::Sample(
               generator_, 16, "abcdefghijlkmnopqrstuvwxyz0123456789") +
           ".json";
  }

 private:
  google::cloud::internal::DefaultPRNG generator_;
};

TEST_F(UnifiedRestCredentialsTest, Insecure) {
  auto credentials = MapCredentials(MakeInsecureCredentials());
  auto header = credentials->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());
  EXPECT_THAT(*header, IsEmpty());
}

TEST_F(UnifiedRestCredentialsTest, AccessToken) {
  auto credentials = MapCredentials(
      MakeAccessTokenCredentials("token1", std::chrono::system_clock::now()));
  for (std::string expected : {"token1", "token1", "token1"}) {
    auto header = credentials->AuthorizationHeader();
    ASSERT_THAT(header, IsOk());
    EXPECT_EQ("Authorization: Bearer " + expected, *header);
  }
}

TEST_F(UnifiedRestCredentialsTest, LoadError) {
  // Create a name for a non-existing file, try to load it, and verify it
  // returns errors.
  auto const filename = TempKeyFileName();
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", filename);

  auto credentials = MapCredentials(MakeGoogleDefaultCredentials());
  EXPECT_THAT(credentials->AuthorizationHeader(), Not(IsOk()));
}

TEST_F(UnifiedRestCredentialsTest, LoadSuccess) {
  // Create a loadable, i.e., syntactically valid, key file, load it, and it
  // has the right contents.
  auto constexpr kKeyId = "test-only-key-id";
  auto constexpr kClientEmail =
      "sa@invalid-test-only-project.iam.gserviceaccount.com";
  auto contents = nlohmann::json{
      {"type", "service_account"},
      {"project_id", "invalid-test-only-project"},
      {"private_key_id", kKeyId},
      {"private_key", google::cloud::storage::testing::kWellFormatedKey},
      {"client_email", kClientEmail},
      {"client_id", "invalid-test-only-client-id"},
      {"auth_uri", "https://accounts.google.com/o/oauth2/auth"},
      {"token_uri", "https://accounts.google.com/o/oauth2/token"},
      {"auth_provider_x509_cert_url",
       "https://www.googleapis.com/oauth2/v1/certs"},
      {"client_x509_cert_url",
       "https://www.googleapis.com/robot/v1/metadata/x509/"
       "foo-email%40foo-project.iam.gserviceaccount.com"},
  };
  auto const filename = TempKeyFileName();
  std::ofstream(filename) << contents.dump(4) << "\n";

  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", filename);

  auto credentials = MapCredentials(MakeGoogleDefaultCredentials());
  // Calling AuthorizationHeader() makes RPCs which would turn this into an
  // integration test, fortunately there are easier ways to verify the file was
  // loaded correctly:
  EXPECT_EQ(kClientEmail, credentials->AccountEmail());
  EXPECT_EQ(kKeyId, credentials->KeyId());

  std::remove(filename.c_str());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
