// Copyright 2021 Google LLC
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

#include "google/cloud/internal/unified_rest_credentials.h"
#include "google/cloud/internal/filesystem.h"
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
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::MakeAccessTokenCredentials;
using ::google::cloud::MakeGoogleDefaultCredentials;
using ::google::cloud::MakeInsecureCredentials;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Eq;
using ::testing::IsEmpty;

// This is an invalidated private key. It was created using the Google Cloud
// Platform console, but then the key (and service account) were deleted.
auto constexpr kWellFormattedKey = R"""(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S
tTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a
6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/
fS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN
eheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP
T4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U
gyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT
Pg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD
2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB
Sqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov
9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG
DiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX
Z23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC
+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2
UimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r
9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5
3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp
Nx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78
LkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des
AgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk
MGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc
W7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe
Mmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7
rE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3
YvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I
lUtj+/nH3HDQjM4ltYfTPUg=
-----END PRIVATE KEY-----
)""";

class UnifiedRestCredentialsTest : public ::testing::Test {
 public:
  UnifiedRestCredentialsTest() : generator_(std::random_device{}()) {}

  std::string TempKeyFileName() {
    return google::cloud::internal::PathAppend(
        ::testing::TempDir(),
        ::google::cloud::internal::Sample(
            generator_, 16, "abcdefghijlkmnopqrstuvwxyz0123456789") +
            ".json");
  }

 private:
  google::cloud::internal::DefaultPRNG generator_;
};

TEST_F(UnifiedRestCredentialsTest, Insecure) {
  auto credentials = MapCredentials(MakeInsecureCredentials());
  auto header = credentials->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());
  EXPECT_THAT(header->first, IsEmpty());
  EXPECT_THAT(header->second, IsEmpty());
}

TEST_F(UnifiedRestCredentialsTest, AccessToken) {
  auto credentials = MapCredentials(
      MakeAccessTokenCredentials("token1", std::chrono::system_clock::now()));
  for (std::string expected : {"token1", "token1", "token1"}) {
    auto header = credentials->AuthorizationHeader();
    ASSERT_THAT(header, IsOk());
    EXPECT_THAT(header->first, Eq("Authorization"));
    EXPECT_THAT(header->second, Eq(std::string{"Bearer "} + expected));
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
      {"private_key", kWellFormattedKey},
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
