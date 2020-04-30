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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/assert_ok.h"
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
 * Helper for decoding base64.
 *
 * Should only be used in tests because it fails if its input isn't proper
 * base64.
 */
std::string Dec64(std::string const& s) {
  auto res = internal::Base64Decode(s);
  return std::string(res.begin(), res.end());
};

/**
 * Test the CreateSignedPolicyDocument function in storage::Client.
 */
class CreateSignedPolicyDocTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
        kJsonKeyfileContents);
    ASSERT_STATUS_OK(creds);
    client.reset(new Client(*creds));
  }

  std::unique_ptr<Client> client;
};

/**
 * Test the RPCs in CreateSignedPolicyDocument function in storage::Client.
 */
class CreateSignedPolicyDocRPCTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{
        std::static_pointer_cast<internal::RawClient>(mock),
        ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                 std::chrono::milliseconds(1), 2.0)});
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
  auto actual =
      client->CreateSignedPolicyDocument(CreatePolicyDocumentForTest());
  ASSERT_STATUS_OK(actual);

  EXPECT_EQ("foo-email@foo-project.iam.gserviceaccount.com", actual->access_id);

  EXPECT_EQ("2010-06-16T11:11:11Z",
            google::cloud::internal::FormatRfc3339(actual->expiration));

  EXPECT_EQ(
      "{"
      "\"conditions\":["
      "[\"starts-with\",\"$key\",\"\"],"
      "{\"acl\":\"bucket-owner-read\"},"
      "{\"bucket\":\"travel-maps\"},"
      "[\"eq\",\"$Content-Type\",\"image/jpeg\"],"
      "[\"content-length-range\",0,1000000]"
      "],"
      "\"expiration\":\"2010-06-16T11:11:11Z\"}",
      Dec64(actual->policy));

  EXPECT_EQ(
      "QoQzyjIedQkiLydcnBvZMvXRlF5yGWgHaEahybtNOZErr6tDqB7pyUCFcGM8aiukSDYVi/"
      "vxQ5YR3YjjTt9khphFOBqBRO5z6/HdX1i9QUGAd3MsTRe9Atlfwx9fj+7sz87Hebv9lJN/"
      "VLRJv7nMuVqGY+QVaXk3krPQNSWJ1cxo+Ip/M7SPP/iFH9O1CnN5QsE7lgLEH/"
      "BdMTaNoblc4XZMfgFZXtxWgi4hSsuAgbGx4ByTlU+BP1cbpfsc1A2Cu8byZtYJQ5cEp7f1+"
      "Kv2zNRqGqYrFWwDhfFHj9t3jj/DuaWycTfpCGfTtOMSB7+rEV87w/vgitFyVS+o0TrrHA==",
      actual->signature);
}

/// @test Verify that CreateSignedPolicyDocument() uses the SignBlob API when
/// needed.
TEST_F(CreateSignedPolicyDocRPCTest, SignRemote) {
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
  auto actual =
      client->CreateSignedPolicyDocument(CreatePolicyDocumentForTest());
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(actual->signature, expected_signed_blob);
}

/// @test Verify that CreateSignedPolicyDocument() + SignBlob() respects retry
/// policies.
TEST_F(CreateSignedPolicyDocRPCTest, SignPolicyTooManyFailures) {
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
TEST_F(CreateSignedPolicyDocRPCTest, SignPolicyPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::SignBlobResponse>(
      *client, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateSignedPolicyDocument(CreatePolicyDocumentForTest())
            .status();
      },
      "SignBlob");
}

#if GOOGLE_CLOUD_CPP_HAVE_CODECVT
PolicyDocumentV4 CreatePolicyDocumentV4ForTest() {
  PolicyDocumentV4 result;
  result.bucket = "test-bucket";
  result.object = "test-object";
  result.expiration = std::chrono::seconds(13);
  result.timestamp =
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z");
  result.conditions.emplace_back(
      PolicyDocumentCondition::StartsWith("Content-Type", "image/"));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ExactMatchObject("bucket", "travel-maps"));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ExactMatch("Content-Disposition", "inline"));
  result.conditions.emplace_back(
      PolicyDocumentCondition::ContentLengthRange(0, 1000000));
  return result;
}

TEST_F(CreateSignedPolicyDocTest, SignV4) {
  auto actual = client->GenerateSignedPostPolicyV4(
      CreatePolicyDocumentV4ForTest(), AddExtensionFieldOption(),
      PredefinedAcl(), Scheme());
  ASSERT_STATUS_OK(actual);

  EXPECT_EQ("https://storage.googleapis.com/test-bucket/", actual->url);
  EXPECT_EQ(
      "foo-email@foo-project.iam.gserviceaccount.com/20100616/auto/storage/"
      "goog4_request",
      actual->access_id);
  EXPECT_EQ("2010-06-16T11:11:24Z",
            google::cloud::internal::FormatRfc3339(actual->expiration));

  EXPECT_EQ(
      "{"
      "\"conditions\":["
      "[\"starts-with\",\"$Content-Type\",\"image/\"],"
      "{\"bucket\":\"travel-maps\"},"
      "[\"eq\",\"$Content-Disposition\",\"inline\"],"
      "[\"content-length-range\",0,1000000],"
      "{\"bucket\":\"test-bucket\"},"
      "{\"key\":\"test-object\"},"
      "{\"x-goog-date\":\"20100616T111111Z\"},"
      "{\"x-goog-credential\":\"foo-email@foo-project.iam.gserviceaccount.com/"
      "20100616/auto/storage/goog4_request\"},"
      "{\"x-goog-algorithm\":\"GOOG4-RSA-SHA256\"}"
      "],"
      "\"expiration\":\"2010-06-16T11:11:24Z\"}",
      Dec64(actual->policy));

  EXPECT_EQ(
      "25b5ef60e9d80fc94ac8c0d94bb8533b6d59de07371091ecf3f698cf465c8d54240a60bf"
      "39840c3e1133d3d07345842809ee97e809a73a801b20ad1a6bcb4d2fb8dfd796b99a85c5"
      "8dde9f76f28d4724543bad012b6f69fd822179c338852d717272313456b895ca95303ced"
      "6fbdee01e23f983df8a594b23a6977b24ff5027a3b491ef2c54fb008cac1eccec15da422"
      "fb6422722edad8e4208e82f8bee82e095441b22a721b8a1d64139958d3fa91739244b203"
      "62998a73258afc68b1bf7bdb9cbeec392829a401e186ec6fb810f647b502005b1742d333"
      "421393b555fc1446f5c6e2b715054f1dd6abbc21b5aade89f17de8edcbae9720bc4bfcb7"
      "ace38d22",
      actual->signature);

  EXPECT_EQ("GOOG4-RSA-SHA256", actual->signing_algorithm);
}

TEST_F(CreateSignedPolicyDocTest, SignV4AddExtensionField) {
  auto actual = client->GenerateSignedPostPolicyV4(
      CreatePolicyDocumentV4ForTest(),
      AddExtensionField("my-field", "my-value"));
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(Dec64(actual->policy), HasSubstr("{\"my-field\":\"my-value\"}"));
}

TEST_F(CreateSignedPolicyDocTest, SignV4PredefinedAcl) {
  auto actual = client->GenerateSignedPostPolicyV4(
      CreatePolicyDocumentV4ForTest(), PredefinedAcl::BucketOwnerRead());
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(Dec64(actual->policy),
              HasSubstr("{\"acl\":\"bucket-owner-read\"}"));
}

TEST_F(CreateSignedPolicyDocTest, SignV4BucketBoundHostname) {
  auto actual = client->GenerateSignedPostPolicyV4(
      CreatePolicyDocumentV4ForTest(), BucketBoundHostname("mydomain.tld"));
  ASSERT_STATUS_OK(actual);

  EXPECT_EQ("https://mydomain.tld/", actual->url);
}

TEST_F(CreateSignedPolicyDocTest, SignV4BucketBoundHostnameHTTP) {
  auto actual = client->GenerateSignedPostPolicyV4(
      CreatePolicyDocumentV4ForTest(), BucketBoundHostname("mydomain.tld"),
      Scheme("http"));
  ASSERT_STATUS_OK(actual);

  EXPECT_EQ("http://mydomain.tld/", actual->url);
}

TEST_F(CreateSignedPolicyDocTest, SignV4VirtualHostname) {
  auto actual = client->GenerateSignedPostPolicyV4(
      CreatePolicyDocumentV4ForTest(), VirtualHostname(true));
  ASSERT_STATUS_OK(actual);

  EXPECT_EQ("https://test-bucket.storage.googleapis.com/", actual->url);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_CODECVT

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
