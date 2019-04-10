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

#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/canonical_errors.h"
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
 * Test the CreateV*SignUrl functions in storage::Client.
 */
class CreateSignedUrlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{std::shared_ptr<internal::RawClient>(mock)});
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

TEST_F(CreateSignedUrlTest, V2Sign) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContents);
  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  auto actual = client.CreateV2SignedUrl("GET", "test-bucket", "test-object");
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(*actual, HasSubstr("test-bucket"));
  EXPECT_THAT(*actual, HasSubstr("test-object"));
}

TEST_F(CreateSignedUrlTest, V2SignBucketOnly) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContents);
  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  auto actual = client.CreateV2SignedUrl("GET", "test-bucket", "", WithAcl());
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(*actual, HasSubstr("test-bucket?GoogleAccessId="));
}

TEST_F(CreateSignedUrlTest, V2SignEscape) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContents);
  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  auto actual = client.CreateV2SignedUrl("GET", "test-bucket", "test+object");
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(*actual, HasSubstr("test-bucket"));
  EXPECT_THAT(*actual, HasSubstr("test%2Bobject"));
}

/// @test Verify that CreateV2SignedUrl() uses the SignBlob API when needed.
TEST_F(CreateSignedUrlTest, V2SignRemote) {
  // Use `echo -n test-signed-blob | openssl base64 -e` to create the magic
  // string.
  std::string expected_signed_blob = "dGVzdC1zaWduZWQtYmxvYg==";
  std::string expected_signed_blob_safe = "dGVzdC1zaWduZWQtYmxvYg%3D%3D";

  EXPECT_CALL(*mock, SignBlob(_))
      .WillOnce(Return(StatusOr<internal::SignBlobResponse>(TransientError())))
      .WillOnce(
          Invoke([&expected_signed_blob](internal::SignBlobRequest const& r) {
            return make_status_or(internal::SignBlobResponse{
                "test-key-id", expected_signed_blob});
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<std::string> actual =
      client.CreateV2SignedUrl("GET", "test-bucket", "test-object");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, HasSubstr(expected_signed_blob_safe));
}

/// @test Verify that CreateV2SignedUrl() + SignBlob() respects retry policies.
TEST_F(CreateSignedUrlTest, V2SignTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::SignBlobResponse>(
      mock, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateV2SignedUrl("GET", "test-bucket", "test-object")
            .status();
      },
      "SignBlob");
}

/// @test Verify that CreateV2SignedUrl() + SignBlob() respects retry policies.
TEST_F(CreateSignedUrlTest, V2SignPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::SignBlobResponse>(
      *client, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateV2SignedUrl("GET", "test-bucket", "test-object")
            .status();
      },
      "SignBlob");
}

// This is a dummy service account JSON file that is inactive. It's fine for it
// to be public.
constexpr char kJsonKeyfileContentsForV4[] = R"""({
  "type": "service_account",
  "project_id": "dummy-project-id",
  "private_key_id": "ffffffffffffffffffffffffffffffffffffffff",
  "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCsPzMirIottfQ2\nryjQmPWocSEeGo7f7Q4/tMQXHlXFzo93AGgU2t+clEj9L5loNhLVq+vk+qmnyDz5\nQ04y8jVWyMYzzGNNrGRW/yaYqnqlKZCy1O3bmnNjV7EDbC/jE1ZLBY0U3HaSHfn6\nS9ND8MXdgD0/ulRTWwq6vU8/w6i5tYsU7n2LLlQTl1fQ7/emO9nYcCFJezHZVa0H\nmeWsdHwWsok0skwQYQNIzP3JF9BpR5gJT2gNge6KopDesJeLoLzaX7cUnDn+CAnn\nLuLDwwSsIVKyVxhBFsFXPplgpaQRwmGzwEbf/Xpt9qo26w2UMgn30jsOaKlSeAX8\ncS6ViF+tAgMBAAECggEACKRuJCP8leEOhQziUx8Nmls8wmYqO4WJJLyk5xUMUC22\nSI4CauN1e0V8aQmxnIc0CDkFT7qc9xBmsMoF+yvobbeKrFApvlyzNyM7tEa/exh8\nDGD/IzjbZ8VfWhDcUTwn5QE9DCoon9m1sG+MBNlokB3OVOt8LieAAREdEBG43kJu\nyQTOkY9BGR2AY1FnAl2VZ/jhNDyrme3tp1sW1BJrawzR7Ujo8DzlVcS2geKA9at7\n55ua5GbHz3hfzFgjVXDfnkWzId6aHypUyqHrSn1SqGEbyXTaleKTc6Pgv0PgkJjG\nhZazWWdSuf1T5Xbs0OhAK9qraoAzT6cXXvMEvvPt6QKBgQDXcZKqJAOnGEU4b9+v\nOdoh+nssdrIOBNMu1m8mYbUVYS1aakc1iDGIIWNM3qAwbG+yNEIi2xi80a2RMw2T\n9RyCNB7yqCXXVKLBiwg9FbKMai6Vpk2bWIrzahM9on7AhCax/X2AeOp+UyYhFEy6\nUFG4aHb8THscL7b515ukSuKb5QKBgQDMq+9PuaB0eHsrmL6q4vHNi3MLgijGg/zu\nAXaPygSYAwYW8KglcuLZPvWrL6OG0+CrfmaWTLsyIZO4Uhdj7MLvX6yK7IMnagvk\nL3xjgxSklEHJAwi5wFeJ8ai/1MIuCn8p2re3CbwISKpvf7Sgs/W4196P4vKvTiAz\njcTiSYFIKQKBgCjMpkS4O0TakMlGTmsFnqyOneLmu4NyIHgfPb9cA4n/9DHKLKAT\noaWxBPgatOVWs7RgtyGYsk+XubHkpC6f3X0+15mGhFwJ+CSE6tN+l2iF9zp52vqP\nQwkjzm7+pdhZbmaIpcq9m1K+9lqPWJRz/3XXuqi+5xWIZ7NaxGvRjqaNAoGAdK2b\nutZ2y48XoI3uPFsuP+A8kJX+CtWZrlE1NtmS7tnicdd19AtfmTuUL6fz0FwfW4Su\nlQZfPT/5B339CaEiq/Xd1kDor+J7rvUHM2+5p+1A54gMRGCLRv92FQ4EON0RC1o9\nm2I4SHysdO3XmjmdXmfp4BsgAKJIJzutvtbqlakCgYB+Cb10z37NJJ+WgjDt+yT2\nyUNH17EAYgWXryfRgTyi2POHuJitd64Xzuy6oBVs3wVveYFM6PIKXlj8/DahYX5I\nR2WIzoCNLL3bEZ+nC6Jofpb4kspoAeRporj29SgesK6QBYWHWX2H645RkRGYGpDo\n51gjy9m/hSNqBbH2zmh04A==\n-----END PRIVATE KEY-----\n",
  "client_email": "test-iam-credentials@dummy-project-id.iam.gserviceaccount.com",
  "client_id": "000000000000000000000",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://oauth2.googleapis.com/token",
  "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
  "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/test-iam-credentials%40dummy-project-id.iam.gserviceaccount.com"
})""";

TEST_F(CreateSignedUrlTest, V4SignGet) {
  // This test uses a disabled key to create a V4 Signed URL for a GET
  // operation. The bucket name was generated at random too.
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContentsForV4);
  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::string const bucket_name = "test-bucket";
  std::string const object_name = "test-object";
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);

  auto actual = client.CreateV4SignedUrl(
      "GET", bucket_name, object_name,
      SignedUrlTimestamp(internal::ParseRfc3339(date)),
      SignedUrlDuration(valid_for),
      AddExtensionHeader("host", "storage.googleapis.com"));
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(*actual, HasSubstr(bucket_name));
  EXPECT_THAT(*actual, HasSubstr(object_name));

  std::string expected =
      "https://storage.googleapis.com/test-bucket/test-object"
      "?X-Goog-Algorithm=GOOG4-RSA-SHA256"
      "&X-Goog-Credential=test-iam-credentials%40dummy-project-id.iam."
      "gserviceaccount.com%2F20190201%2Fauto%2Fstorage%2Fgoog4_request"
      "&X-Goog-Date=20190201T090000Z"
      "&X-Goog-Expires=10"
      "&X-Goog-SignedHeaders=host"
      "&X-Goog-Signature="
      "95e6a13d43a1d1962e667f17397f2b80ac9bdd1669210d5e08e0135df9dff4e56113485d"
      "be429ca2266487b9d1796ebdee2d7cf682a6ef3bb9fbb4c351686fba90d7b621cf1c4eb1"
      "fdf126460dd25fa0837dfdde0a9fd98662ce60844c458448fb2b352c203d9969cb74efa4"
      "bdb742287744a4f2308afa4af0e0773f55e32e92973619249214b97283b2daa141952444"
      "44e33f938138d1e5f561088ce8011f4986dda33a556412594db7c12fc40e1ff3f1bedeb7"
      "a42f5bcda0b9567f17f65855f65071fabb88ea12371877f3f77f10e1466fff6ff6973b74"
      "a933322ff0949ce357e20abe96c3dd5cfab42c9c83e740a4d32b9e11e146f0eb3404d2e9"
      "75896f74";

  EXPECT_EQ(expected, *actual);
}

TEST_F(CreateSignedUrlTest, V4SignPut) {
  // This test uses a disabled key to create a V4 Signed URL for a PUT
  // operation. The bucket name was generated at random too.
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContentsForV4);
  ASSERT_STATUS_OK(creds);
  Client client(*creds);

  std::string const bucket_name = "test-bucket";
  std::string const object_name = "test-object";
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);

  auto actual = client.CreateV4SignedUrl(
      "POST", bucket_name, object_name,
      SignedUrlTimestamp(internal::ParseRfc3339(date)),
      SignedUrlDuration(valid_for),
      AddExtensionHeader("host", "storage.googleapis.com"),
      AddExtensionHeader("x-goog-resumable", "start"));
  ASSERT_STATUS_OK(actual);

  EXPECT_THAT(*actual, HasSubstr(bucket_name));
  EXPECT_THAT(*actual, HasSubstr(object_name));

  std::string expected =
      "https://storage.googleapis.com/test-bucket/test-object"
      "?X-Goog-Algorithm=GOOG4-RSA-SHA256"
      "&X-Goog-Credential=test-iam-credentials%40dummy-project-id.iam."
      "gserviceaccount.com%2F20190201%2Fauto%2Fstorage%2Fgoog4_request"
      "&X-Goog-Date=20190201T090000Z"
      "&X-Goog-Expires=10"
      "&X-Goog-SignedHeaders=host%3Bx-goog-resumable"
      "&X-Goog-Signature="
      "4a6d39b23343cedf4c30782aed4b384001828c79ffa3a080a481ea01a640dea0a0ceb58d"
      "67a12cef3b243c3f036bb3799c6ee88e8db3eaf7d0bdd4b70a228d0736e07eaa1ee076af"
      "f5c6ce09dff1f1f03a0d8ead0d2893408dd3604fdabff553aa6d7af2da67cdba6790006a"
      "70240f96717b98f1a6ccb24f00940749599be7ef72aaa5358db63ddd54b2de9e2d6d6a58"
      "6eac4fe25f36d86fc6ab150418e9c6fa01b732cded226c6d62fc95b72473a4cc55a82574"
      "82583fe66d9ab6ede909eb41516a8690946c3e87b0f2052eb0e97e012a14b2f721c42e6e"
      "19b8a1cd5658ea36264f10b9b1ada66b8ed5bf7ed7d1708377ac6e5fe608ae361fb594d2"
      "e5b24c54";

  EXPECT_EQ(expected, *actual);
}

/// @test Verify that CreateV4SignedUrl() uses the SignBlob API when needed.
TEST_F(CreateSignedUrlTest, V4SignRemote) {
  auto creds = oauth2::CreateServiceAccountCredentialsFromJsonContents(
      kJsonKeyfileContents);
  ASSERT_STATUS_OK(creds);
  // Use `echo -n test-signed-blob | openssl base64 -e` to create the magic
  // string.
  std::string expected_signed_blob = "dGVzdC1zaWduZWQtYmxvYg==";
  // Use `echo -n test-signed-blob | od -x` to create the magic string.
  std::string expected_signed_blob_hex = "746573742d7369676e65642d626c6f62";

  EXPECT_CALL(*mock, SignBlob(_))
      .WillOnce(Return(StatusOr<internal::SignBlobResponse>(TransientError())))
      .WillOnce(
          Invoke([&expected_signed_blob](internal::SignBlobRequest const& r) {
            return make_status_or(internal::SignBlobResponse{
                "test-key-id", expected_signed_blob});
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<std::string> actual =
      client.CreateV4SignedUrl("GET", "test-bucket", "test-object");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, HasSubstr(expected_signed_blob_hex));
}

/// @test Verify that CreateV4SignedUrl() + SignBlob() respects retry policies.
TEST_F(CreateSignedUrlTest, V4SignTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::SignBlobResponse>(
      mock, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateV4SignedUrl("GET", "test-bucket", "test-object")
            .status();
      },
      "SignBlob");
}

/// @test Verify that CreateV4SignedUrl() + SignBlob() respects retry policies.
TEST_F(CreateSignedUrlTest, V4SignPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::SignBlobResponse>(
      *client, EXPECT_CALL(*mock, SignBlob(_)),
      [](Client& client) {
        return client.CreateV4SignedUrl("GET", "test-bucket", "test-object")
            .status();
      },
      "SignBlob");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
