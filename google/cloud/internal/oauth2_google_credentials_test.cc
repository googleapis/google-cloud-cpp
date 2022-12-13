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

#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/oauth2_anonymous_credentials.h"
#include "google/cloud/internal/oauth2_authorized_user_credentials.h"
#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/oauth2_external_account_credentials.h"
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::WhenDynamicCastTo;

using MockHttpClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

class GoogleCredentialsTest : public ::testing::Test {
 public:
  // Make sure other higher-precedence credentials (ADC env var, gcloud ADC from
  // well-known path) aren't loaded.
  GoogleCredentialsTest()
      : home_env_var_(GoogleAdcHomeEnvVar(), {}),
        adc_env_var_(GoogleAdcEnvVar(), {}),
        gcloud_path_override_env_var_(GoogleGcloudAdcFileEnvVar(), {}) {}

 protected:
  ScopedEnvironment home_env_var_;
  ScopedEnvironment adc_env_var_;
  ScopedEnvironment gcloud_path_override_env_var_;
};

auto constexpr kAuthorizedUserCredContents = R"""({
  "client_id": "test-invalid-test-invalid.apps.googleusercontent.com",
  "client_secret": "invalid-invalid-invalid",
  "refresh_token": "1/test-test-test",
  "type": "authorized_user"
})""";

auto constexpr kServiceAccountCredContents = R"""({
    "type": "service_account",
    "project_id": "foo-project",
    "private_key_id": "a1a111aa1111a11a11a11aa111a111a1a1111111",
    "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S\ntTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a\n6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/\nfS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN\neheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP\nT4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U\ngyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT\nPg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD\n2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB\nSqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov\n9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG\nDiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX\nZ23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC\n+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2\nUimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r\n9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5\n3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp\nNx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78\nLkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des\nAgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk\nMGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc\nW7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe\nMmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7\nrE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3\nYvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I\nlUtj+/nH3HDQjM4ltYfTPUg=\n-----END PRIVATE KEY-----\n",
    "client_email": "foo-email@foo-project.iam.gserviceaccount.com",
    "client_id": "100000000000000000001",
    "auth_uri": "https://accounts.google.com/o/oauth2/auth",
    "token_uri": "https://accounts.google.com/o/oauth2/token",
    "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
    "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/foo-email%40foo-project.iam.gserviceaccount.com"
})""";

auto constexpr kExternalAccountContents = R"""({
  "type": "external_account",
  "audience": "test-audience",
  "subject_token_type": "test-subject-token-type",
  "token_url": "https://sts.example.com/",
  "credential_source": {"url": "https://subject.example.com/"}
})""";

std::string TempFileName() {
  static auto generator =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(),
      ::google::cloud::internal::Sample(
          generator, 16, "abcdefghijlkmnopqrstuvwxyz0123456789") +
          ".json");
}

/**
 * @test Verify `GoogleDefaultCredentials()` loads authorized user credentials.
 *
 * This test only verifies the right type of object is created, the unit tests
 * for `AuthorizedUserCredentials` already check that once loaded the class
 * works correctly. Testing here would be redundant. Furthermore, calling
 * `AuthorizationHeader()` initiates the key verification workflow, that
 * requires valid keys and contacting Google's production servers, and would
 * make this an integration test.
 */
TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsViaEnvVar) {
  auto const filename = TempFileName();
  std::ofstream(filename) << kAuthorizedUserCredContents;
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  // Test that the authorized user credentials are loaded as the default when
  // specified via the well known environment variable.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<AuthorizedUserCredentials*>(NotNull()));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsViaGcloudFile) {
  auto const filename = TempFileName();
  std::ofstream(filename) << kAuthorizedUserCredContents;
  auto const env =
      ScopedEnvironment(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  // Test that the authorized user credentials are loaded as the default when
  // stored in the well known gcloud ADC file path.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<AuthorizedUserCredentials*>(NotNull()));
}

TEST_F(GoogleCredentialsTest, LoadValidExternalAccountCredentialsViaEnvVar) {
  auto const filename = TempFileName();
  std::ofstream(filename) << kExternalAccountContents;
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  // Test that the authorized user credentials are loaded as the default when
  // specified via the well known environment variable.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<ExternalAccountCredentials*>(NotNull()));
}

TEST_F(GoogleCredentialsTest,
       LoadValidExternalAccountCredentialsViaGcloudFile) {
  auto const filename = TempFileName();
  std::ofstream(filename) << kExternalAccountContents;
  auto const env =
      ScopedEnvironment(GoogleGcloudAdcFileEnvVar(), filename.c_str());

  // Test that the authorized user credentials are loaded as the default when
  // stored in the well known gcloud ADC file path.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<ExternalAccountCredentials*>(NotNull()));
}

/**
 * @test Verify `GoogleDefaultCredentials()` loads service account credentials.
 *
 * This test only verifies the right type of object is created, the unit tests
 * for `ServiceAccountCredentials` already check that once loaded the class
 * works correctly. Testing here would be redundant. Furthermore, calling
 * `AuthorizationHeader()` initiates the key verification workflow, that
 * requires valid keys and contacting Google's production servers, and would
 * make this an integration test.
 */

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsViaEnvVar) {
  auto const filename = TempFileName();
  std::ofstream(filename) << kServiceAccountCredContents;
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<ServiceAccountCredentials*>(NotNull()));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsViaGcloudFile) {
  auto const filename = TempFileName();
  std::ofstream(filename) << kServiceAccountCredContents;
  auto const env =
      ScopedEnvironment(GoogleGcloudAdcFileEnvVar(), filename.c_str());

  // Test that the service account credentials are loaded as the default when
  // stored in the well known gcloud ADC file path.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<ServiceAccountCredentials*>(NotNull()));
}

TEST_F(GoogleCredentialsTest, LoadComputeEngineCredentialsFromADCFlow) {
  // Developers may have an ADC file in $HOME/.gcloud, override the default
  // path to a location that is not going to succeed.
  auto const filename = TempFileName();
  auto const env = ScopedEnvironment(GoogleGcloudAdcFileEnvVar(), filename);
  // If the ADC flow thinks we're on a GCE instance, it should return
  // ComputeEngineCredentials.
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  ASSERT_STATUS_OK(creds);
  EXPECT_THAT(creds->get(),
              WhenDynamicCastTo<ComputeEngineCredentials*>(NotNull()));
}

TEST_F(GoogleCredentialsTest, LoadUnknownTypeCredentials) {
  auto const filename = TempFileName();
  std::ofstream(filename) << R"""({"type": "unknown_type"})""";
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());
  EXPECT_THAT(creds, StatusIs(Not(StatusCode::kOk),
                              AllOf(HasSubstr("Unsupported credential type"),
                                    HasSubstr(filename))));
}

TEST_F(GoogleCredentialsTest, LoadInvalidCredentials) {
  auto const filename = TempFileName();
  std::ofstream(filename) << R"""( not-a-json-object-string )""";
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  EXPECT_THAT(creds, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("credentials file " + filename)));
  (void)std::remove(filename.c_str());
}

TEST_F(GoogleCredentialsTest, LoadInvalidAuthorizedUserCredentialsViaADC) {
  auto const filename = TempFileName();
  std::ofstream(filename) << R"""("type": "authorized_user")""";
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  EXPECT_THAT(creds, StatusIs(StatusCode::kInvalidArgument));
  (void)std::remove(filename.c_str());
}

TEST_F(GoogleCredentialsTest, LoadInvalidServiceAccountCredentialsViaADC) {
  auto const filename = TempFileName();
  std::ofstream(filename) << R"""("type": "service_account")""";
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename.c_str());

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  EXPECT_THAT(creds, StatusIs(StatusCode::kInvalidArgument));
  (void)std::remove(filename.c_str());
}

TEST_F(GoogleCredentialsTest, MissingCredentialsViaEnvVar) {
  auto const filename = TempFileName();
  auto const env = ScopedEnvironment(GoogleAdcEnvVar(), filename);

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto creds =
      GoogleDefaultCredentials(Options{}, client_factory.AsStdFunction());
  EXPECT_THAT(creds, StatusIs(Not(StatusCode::kOk),
                              AllOf(HasSubstr("Cannot open credentials file"),
                                    HasSubstr(filename))));
}

TEST_F(GoogleCredentialsTest, MissingCredentialsViaGcloudFilePath) {
  auto const filename = TempFileName();

  // The method to create default credentials should see that no file exists at
  // this path, then continue trying to load the other credential types,
  // eventually finding no valid credentials and hitting a runtime error.
  auto const adc_env = ScopedEnvironment(GoogleGcloudAdcFileEnvVar(), filename);
  auto const gce_host_env = ScopedEnvironment(
      internal::GceMetadataHostnameEnvVar(), "invalid.google.internal");

  auto const options = Options{}.set<UserProjectOption>("test-only");
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillRepeatedly([](Options const& o) {
    EXPECT_EQ(o.get<UserProjectOption>(), "test-only");
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get).WillOnce([] {
      return Status{StatusCode::kUnavailable, "bad hostname"};
    });
    return mock;
  });
  auto creds =
      GoogleDefaultCredentials(options, client_factory.AsStdFunction());
  ASSERT_STATUS_OK(creds);
  ASSERT_THAT(*creds, NotNull());
  auto token = (*creds)->GetToken(std::chrono::system_clock::now());
  EXPECT_THAT(token,
              StatusIs(StatusCode::kUnavailable, HasSubstr("bad hostname")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
