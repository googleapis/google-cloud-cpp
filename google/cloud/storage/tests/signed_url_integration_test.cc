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
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_bucket_name;
char const* flag_service_account;

class SignedUrlIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(SignedUrlIntegrationTest, CreateV2SignedUrlGet) {
  if (UsingTestbench()) {
    // The testbench does not implement signed URLs.
    return;
  }
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  std::string service_account = flag_service_account;

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client->InsertObject(bucket_name, object_name, expected,
                                   IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  StatusOr<std::string> signed_url = client->CreateV2SignedUrl(
      "GET", bucket_name, object_name, SigningAccount(service_account));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());

  auto response = builder.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  EXPECT_EQ(expected, response->payload);

  auto deleted = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_F(SignedUrlIntegrationTest, CreateV2SignedUrlPut) {
  if (UsingTestbench()) {
    // The testbench does not implement signed URLs.
    return;
  }
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  std::string service_account = flag_service_account;

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  StatusOr<std::string> signed_url = client->CreateV2SignedUrl(
      "PUT", bucket_name, object_name, SigningAccount(service_account),
      ContentType("application/octet-stream"));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");
  builder.AddHeader("content-type: application/octet-stream");

  auto response = builder.BuildRequest().MakeRequest(expected);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto deleted = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_F(SignedUrlIntegrationTest, CreateV4SignedUrlGet) {
  if (UsingTestbench()) {
    // The testbench does not implement signed URLs.
    return;
  }
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  std::string service_account = flag_service_account;

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client->InsertObject(bucket_name, object_name, expected,
                                   IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  StatusOr<std::string> signed_url = client->CreateV4SignedUrl(
      "GET", bucket_name, object_name, SigningAccount(service_account));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());

  auto response = builder.BuildRequest().MakeRequest(std::string{});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  EXPECT_EQ(expected, response->payload);

  auto deleted = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_F(SignedUrlIntegrationTest, CreateV4SignedUrlPut) {
  if (UsingTestbench()) {
    // The testbench does not implement signed URLs.
    return;
  }
  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  std::string bucket_name = flag_bucket_name;
  std::string service_account = flag_service_account;

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  StatusOr<std::string> signed_url = client->CreateV4SignedUrl(
      "PUT", bucket_name, object_name, SigningAccount(service_account));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");

  auto response = builder.BuildRequest().MakeRequest(expected);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  auto stream = client->ReadObject(bucket_name, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto deleted = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(deleted);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <bucket-name> <service-account-name>\n";
    return 1;
  }

  google::cloud::storage::flag_bucket_name = argv[1];
  google::cloud::storage::flag_service_account = argv[2];

  return RUN_ALL_TESTS();
}
