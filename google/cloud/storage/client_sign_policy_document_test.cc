// Copyright 2019 Google LLC
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

#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr char kJsonKeyfileContents[] = R"""({
      "type": "service_account",
      "project_id": "foo-project",
      "private_key_id": "a1a111aa1111a11a11a11aa111a111a1a1111111",
      "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S\ntTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a\n6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/\nfS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN\neheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP\nT4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U\ngyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT\nPg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD\n2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB\nSqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov\n9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG\nDiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX\nZ23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC\n+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2\nUimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r\n9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5\n3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp\nNx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78\nLkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des\nAgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk\nMGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc\nW7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe\nMmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7\nrE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3\nYvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I\nlUtj+/nH3HDQjM4ltYfTPUg=\n-----END PRIVATE KEY-----\n",
      "client_email": "foo-email@foo-project.iam.gserviceaccount.com",
      "client_id": "100000000000000000001",
      "auth_uri": "https://accounts.google.com/o/oauth2/auth",
      "token_uri": "https://oauth2.googleapis.com/token",
      "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
      "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/foo-email%40foo-project.iam.gserviceaccount.com"
})""";

/**
 * Test the CreateSignedPolicyDocument function in storage::Client.
 */
class CreateSignedPolicyDocTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(
        new Client{std::static_pointer_cast<internal::RawClient>(mock)});
  }
  void TearDown() override {
    client.reset();
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
  std::unique_ptr<Client> client;
  ClientOptions client_options =
      ClientOptions(oauth2::CreateAnonymousCredentials());
};

PolicyDocument CreatePolicyDocumentForTest() {
  PolicyDocument result;
  result.expiration =
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z");
  result.conditions.emplace_back(
      PolicyDocumentCondition::StartsWith("key", ""));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ExactMatchObject("acl", "bucket-owner-read"));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ExactMatchObject("bucket", "travel-maps"));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ExactMatch("Content-Type", "image/jpeg"));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ContentLengthRange(0, 1000000));
  return result;
}

TEST_F(CreateSignedPolicyDocTest, Sign) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContents);
  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  auto actual =
      client.CreateSignedPolicyDocument(CreatePolicyDocumentForTest());
  ASSERT_STATUS_OK(actual);

  EXPECT_EQ("foo-email@foo-project.iam.gserviceaccount.com", actual->access_id);

  EXPECT_EQ("2010-06-16T11:11:11Z",
            google::cloud::internal::FormatRfc3339(actual->expiration));

  EXPECT_EQ(
      "eyJjb25kaXRpb25zIjpbeyJhY2wiOiJidWNrZXQtb3duZXItcmVhZCJ9LHsiYnVja2V0Ijoi"
      "dHJhdmVsLW1hcHMifSxbImNvbnRlbnQtbGVuZ3RoLXJhbmdlIiwwLDEwMDAwMDBdXSwiZXhw"
      "aXJhdGlvbiI6IjIwMTAtMDYtMTZUMTE6MTE6MTFaIn0=",
      actual->policy);

  EXPECT_EQ(
      "kFWvHSh72uILi3XyvLe7dFzL4mzyHjEYHMwAo1UnfKTzfX7fcnuPa0jRWym8fAg5Q9dSnVrx"
      "PWXKbIaxk1UmlQ992iMhwtgEFpGyc+znRXJBX/"
      "CeDOj7i4t41RScLBzEcdPdGLm+"
      "tyGM79SoBacJMmmw3gTZIrv4ASFp7we784tEpXBcAF1AcY4hKUfMX87cyqdH/"
      "s+YKWuow7CJwpHJoC0QogkPpUCW2gt0tpZtRQZn5Beo9imFIvqG0Qkan/"
      "nCxBpgTP3b9AzebOx1XQb6wi1QWWBjIFtPHyCOI7alpu8XNlN61jQNo/"
      "MQcW2OKVKgjcQS6vnLm+MJiAo+sslUtg==",
      actual->signature);
}

/// @test Verify that CreateSignedPolicyDocument() uses the SignBlob API when
/// needed.
TEST_F(CreateSignedPolicyDocTest, SignRemote) {
  // Use `echo -n test-signed-blob | openssl base64 -e` to create the magic
  // string.
  std::string expected_signed_blob = "dGVzdC1zaWduZWQtYmxvYg==";

  EXPECT_CALL(*mock, SignBlob(_))
      .WillOnce(Return(StatusOr<internal::SignBlobResponse>(TransientError())))
      .WillOnce(
          Invoke([&expected_signed_blob](internal::SignBlobRequest const&) {
            return make_status_or(internal::SignBlobResponse{
                "test-key-id", expected_signed_blob});
          }));
  Client client{std::static_pointer_cast<internal::RawClient>(mock)};

  auto actual =
      client.CreateSignedPolicyDocument(CreatePolicyDocumentForTest());
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(actual->signature, expected_signed_blob);
}

/// @test Verify that CreateSignedPolicyDocument() + SignBlob() respects retry
/// policies.
TEST_F(CreateSignedPolicyDocTest, SignPolicyTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::SignBlobResponse>(
      mock, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateSignedPolicyDocument(CreatePolicyDocumentForTest())
            .status();
      },
      "SignBlob");
}

/// @test Verify that CreateSignedPolicyDocument() + SignBlob() respects retry
/// policies.
TEST_F(CreateSignedPolicyDocTest, SignPolicyPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::SignBlobResponse>(
      *client, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateSignedPolicyDocument(CreatePolicyDocumentForTest())
            .status();
      },
      "SignBlob");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
