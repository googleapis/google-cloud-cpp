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

#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {

using ::google::cloud::internal::SetEnv;
using ::google::cloud::internal::UnsetEnv;
using ::google::cloud::storage::internal::GceCheckOverrideEnvVar;
using ::google::cloud::testing_util::EnvironmentVariableRestore;
using ::testing::HasSubstr;

class GoogleCredentialsTest : public ::testing::Test {
 public:
  GoogleCredentialsTest()
      : home_env_var_(GoogleAdcHomeEnvVar()),
        adc_env_var_(GoogleAdcEnvVar()),
        gcloud_path_override_env_var_(GoogleGcloudAdcFileEnvVar()),
        gce_check_override_env_var_(GceCheckOverrideEnvVar()) {}

 protected:
  void SetUp() override {
    home_env_var_.SetUp();
    adc_env_var_.SetUp();
    gcloud_path_override_env_var_.SetUp();
    gce_check_override_env_var_.SetUp();
  }

  void TearDown() override {
    gce_check_override_env_var_.TearDown();
    gcloud_path_override_env_var_.TearDown();
    adc_env_var_.TearDown();
    home_env_var_.TearDown();
  }

 protected:
  EnvironmentVariableRestore home_env_var_;
  EnvironmentVariableRestore adc_env_var_;
  EnvironmentVariableRestore gcloud_path_override_env_var_;
  EnvironmentVariableRestore gce_check_override_env_var_;
};

// TODO(#3022): move these constants and WriteBase64AsBinary to a common place.

// This is a base64-encoded p12 key-file. The service account was deleted
// after creating the key-file, so the key was effectively invalidated, but
// the format is correct, so we can use it to verify that p12 key-files can be
// loaded.
//
// If you want to change the file (for whatever reason), these commands are
// helpful:
//    Generate a new key:
//      gcloud iam service-accounts keys create /dev/shm/key.p12
//          --key-file-type=p12 --iam-account=${SERVICE_ACCOUNT}
//    Base64 encode the key (then cut&paste) here:
//      openssl base64 -e < /dev/shm/key.p12
//    Find the service account ID:
//      openssl pkcs12 -in /dev/shm/key.p12
//          -passin pass:notasecret  -nodes -info  | grep =CN
//    Delete the service account ID:
//      gcloud iam service-accounts delete --quiet ${SERVICE_ACCOUNT}
char const kP12ServiceAccountId[] = "104849618361176160538";
char const kP12KeyFileContents[] =
    "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";

void WriteBase64AsBinary(std::string const& filename, char const* data) {
  std::ofstream os(filename, std::ios::binary);
  os.exceptions(std::ios::badbit);
  auto bytes = internal::Base64Decode(data);
  for (unsigned char c : bytes) {
    os << c;
  }
  os.close();
}

std::string const AUTHORIZED_USER_CRED_FILENAME = "authorized-user.json";
std::string const AUTHORIZED_USER_CRED_CONTENTS = R"""({
  "client_id": "test-invalid-test-invalid.apps.googleusercontent.com",
  "client_secret": "invalid-invalid-invalid",
  "refresh_token": "1/test-test-test",
  "type": "authorized_user"
})""";

void SetupAuthorizedUserCredentialsFileForTest(std::string const& filename) {
  std::ofstream os(filename);
  os << AUTHORIZED_USER_CRED_CONTENTS;
  os.close();
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
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);

  // Test that the authorized user credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  // Need to create a temporary for the pointer because clang-tidy warns about
  // using expressions with (potential) side-effects inside typeid().
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsViaGcloudFile) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);
  // Test that the authorized user credentials are loaded as the default when
  // stored in the the well known gcloud ADC file path.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsFromFilename) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);
  auto creds = CreateAuthorizedUserCredentialsFromJsonFilePath(filename);
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsFromContents) {
  // Test that the authorized user credentials are loaded from a string
  // representing JSON contents.
  auto creds = CreateAuthorizedUserCredentialsFromJsonContents(
      AUTHORIZED_USER_CRED_CONTENTS);
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadInvalidAuthorizedUserCredentialsFromFilename) {
  std::string filename = ::testing::TempDir() + "invalid-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""( not-a-json-object-string )""";
  os << contents_str;
  os.close();
  auto creds = CreateAuthorizedUserCredentialsFromJsonFilePath(filename);
  ASSERT_FALSE(creds) << "status=" << creds.status();
}

TEST_F(GoogleCredentialsTest,
       LoadInvalidAuthorizedUserCredentialsFromJsonContents) {
  std::string contents_str = R"""( not-a-json-object-string )""";
  auto creds = CreateAuthorizedUserCredentialsFromJsonContents(contents_str);
  ASSERT_FALSE(creds) << "status=" << creds.status();
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

std::string const SERVICE_ACCOUNT_CRED_FILENAME = "service-account.json";
std::string const SERVICE_ACCOUNT_CRED_CONTENTS = R"""({
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

void SetupServiceAccountCredentialsFileForTest(std::string const& filename) {
  std::ofstream os(filename);
  os << SERVICE_ACCOUNT_CRED_CONTENTS;
  os.close();
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsViaEnvVar) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  // Need to create a temporary for the pointer because clang-tidy warns about
  // using expressions with (potential) side-effects inside typeid().
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsViaGcloudFile) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // stored in the the well known gcloud ADC file path.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsFromFilename) {
  std::string filename = ::testing::TempDir() + SERVICE_ACCOUNT_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded from a file.
  auto creds = CreateServiceAccountCredentialsFromJsonFilePath(filename);
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));

  // Test the wrapper function.
  creds = CreateServiceAccountCredentialsFromFilePath(filename);
  ASSERT_STATUS_OK(creds);
  auto* ptr2 = creds->get();
  EXPECT_EQ(typeid(*ptr2), typeid(ServiceAccountCredentials<>));

  creds = CreateServiceAccountCredentialsFromFilePath(
      filename, {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr3 = creds->get();
  EXPECT_EQ(typeid(*ptr3), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromFilenameWithOptionalArgs) {
  std::string filename = ::testing::TempDir() + SERVICE_ACCOUNT_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded from a file.
  auto creds = CreateServiceAccountCredentialsFromJsonFilePath(
      filename, {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadInvalidServiceAccountCredentialsFromFilename) {
  std::string filename = ::testing::TempDir() + "invalid-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""( not-a-json-object-string )""";
  os << contents_str;
  os.close();

  auto creds = CreateServiceAccountCredentialsFromJsonFilePath(
      filename, {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_FALSE(creds) << "status=" << creds.status();
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromDefaultPathsViaEnvVar) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_STATUS_OK(creds);
  // Need to create a temporary for the pointer because clang-tidy warns about
  // using expressions with (potential) side-effects inside typeid().
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromDefaultPathsViaGcloudFile) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // stored in the the well known gcloud ADC file path.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromDefaultPathsWithOptionalArgs) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths(
      {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(
    GoogleCredentialsTest,
    DoNotLoadAuthorizedUserCredentialsFromCreateServiceAccountCredentialsFromDefaultPaths) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);

  // Test that the authorized user credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Unsupported credential type"));
}

TEST_F(GoogleCredentialsTest,
       MissingCredentialsCreateServiceAccountCredentialsFromDefaultPaths) {
  // Make sure other higher-precedence credentials (ADC env var, gcloud ADC from
  // well-known path) aren't loaded.
  UnsetEnv(GoogleAdcEnvVar());
  // The developer may have configured something that are not service account
  // credentials in the well-known path. Change the search location to a
  // directory that should have have developer configuration files.
  SetEnv(GoogleAdcHomeEnvVar(), ::testing::TempDir());
  // Test that when CreateServiceAccountCredentialsFromDefaultPaths cannot
  // find any credentials, it fails.
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Could not create service account credentials"));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsFromContents) {
  // Test that the service account credentials are loaded from a string
  // representing JSON contents.
  auto creds = CreateServiceAccountCredentialsFromJsonContents(
      SERVICE_ACCOUNT_CRED_CONTENTS,
      {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadInvalidServiceAccountCredentialsFromContents) {
  // Test that providing invalid contents returns a failure status.
  auto creds = CreateServiceAccountCredentialsFromJsonContents(
      "not-a-valid-jason-object",
      {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_FALSE(creds) << "status=" << creds.status();
}

TEST_F(GoogleCredentialsTest, LoadComputeEngineCredentialsFromADCFlow) {
  // Make sure other higher-precedence credentials (ADC env var, gcloud ADC from
  // well-known path) aren't loaded.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), "");
  // If the ADC flow thinks we're on a GCE instance, it should return
  // ComputeEngineCredentials.
  SetEnv(GceCheckOverrideEnvVar(), "1");

  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ComputeEngineCredentials<>));
}

TEST_F(GoogleCredentialsTest, CreateComputeEngineCredentialsWithDefaultEmail) {
  auto credentials = CreateComputeEngineCredentials();
  auto* ptr = credentials.get();
  EXPECT_EQ(typeid(*ptr), typeid(ComputeEngineCredentials<>));
  EXPECT_EQ(
      std::string("default"),
      dynamic_cast<ComputeEngineCredentials<>*>(ptr)->service_account_email());
}

TEST_F(GoogleCredentialsTest, CreateComputeEngineCredentialsWithExplicitEmail) {
  auto credentials = CreateComputeEngineCredentials("foo@bar.baz");
  auto* ptr = credentials.get();
  EXPECT_EQ(typeid(*ptr), typeid(ComputeEngineCredentials<>));
  EXPECT_EQ(
      std::string("foo@bar.baz"),
      dynamic_cast<ComputeEngineCredentials<>*>(ptr)->service_account_email());
}

TEST_F(GoogleCredentialsTest, LoadUnknownTypeCredentials) {
  std::string filename = ::testing::TempDir() + "unknown-type-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""({
  "type": "unknown_type"
})""";
  os << contents_str;
  os.close();
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Unsupported credential type"));
  EXPECT_THAT(creds.status().message(), HasSubstr(filename));
}

TEST_F(GoogleCredentialsTest, LoadInvalidCredentials) {
  std::string filename = ::testing::TempDir() + "invalid-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""( not-a-json-object-string )""";
  os << contents_str;
  os.close();
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_EQ(StatusCode::kInvalidArgument, creds.status().code());
  EXPECT_THAT(creds.status().message(),
              HasSubstr("credentials file " + filename));
}

TEST_F(GoogleCredentialsTest, LoadInvalidAuthorizedUserCredentialsViaADC) {
  std::string filename = ::testing::TempDir() + "invalid-au-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""("type": "authorized_user")""";
  os << contents_str;
  os.close();
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_EQ(StatusCode::kInvalidArgument, creds.status().code());
}

TEST_F(GoogleCredentialsTest, LoadInvalidServiceAccountCredentialsViaADC) {
  std::string filename = ::testing::TempDir() + "invalid-au-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""("type": "service_account")""";
  os << contents_str;
  os.close();
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_EQ(StatusCode::kInvalidArgument, creds.status().code());
}

TEST_F(GoogleCredentialsTest, MissingCredentialsViaEnvVar) {
  char const filename[] = "missing-credentials.json";
  SetEnv(GoogleAdcEnvVar(), filename);

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Cannot open credentials file"));
  EXPECT_THAT(creds.status().message(), HasSubstr(filename));
}

TEST_F(GoogleCredentialsTest, MissingCredentialsViaGcloudFilePath) {
  char const filename[] = "missing-credentials.json";

  // Make sure other credentials (ADC env var, implicit environment-based creds)
  // aren't found either.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GceCheckOverrideEnvVar(), "0");
  // The method to create default credentials should see that no file exists at
  // this path, then continue trying to load the other credential types,
  // eventually finding no valid credentials and hitting a runtime error.
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename);

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Could not automatically determine"));
}

TEST_F(GoogleCredentialsTest, LoadP12Credentials) {
  // Ensure that the parser detects that it's not a JSON file and parses it as
  // a P12 file.
  std::string filename = ::testing::TempDir() + "credentials.p12";
  WriteBase64AsBinary(filename, kP12KeyFileContents);
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
  EXPECT_EQ(kP12ServiceAccountId, ptr->AccountEmail());
  EXPECT_FALSE(ptr->KeyId().empty());
  EXPECT_EQ(0, std::remove(filename.c_str()));
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
