// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/iam/iam_credentials_client.gcpcxx.pb.h"
#include "google/cloud/testing_util/assert_ok.h"
//#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace iam {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(IamCredentialsIntegrationTest, GenerateAccessTokenSuccess) {
  google::protobuf::Duration lifetime;
  lifetime.set_seconds(3600);
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateAccessToken(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, {"https://www.googleapis.com/auth/spanner.admin"}, lifetime);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->access_token().size(), 0);
}

TEST(IamCredentialsIntegrationTest, GenerateAccessTokenFailure) {
  google::protobuf::Duration lifetime;
  lifetime.set_seconds(3600);
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateAccessToken(
      "projects/-/serviceAccounts/"
      "bob@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, {"https://www.googleapis.com/auth/spanner.admin"}, lifetime);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, GenerateIdTokenSuccess) {
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateIdToken(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, {"https://www.googleapis.com/auth/spanner.admin"}, true);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->token().size(), 0);
}

TEST(IamCredentialsIntegrationTest, GenerateIdTokenFailure) {
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateIdToken(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, {""}, false);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, SignBlobSuccess) {
  std::string payload = "somebytes";
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignBlob(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, payload);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->key_id().size(), 0);
  EXPECT_GT(response->signed_blob().size(), 0);
}

TEST(IamCredentialsIntegrationTest, SignBlobFailure) {
  std::string payload = "somebytes";
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignBlob(
      "projects/-/serviceAccounts/"
      "bob@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, payload);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, SignJwtSuccess) {
  std::string payload = R"({"some": "json"})";
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignJwt(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, payload);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->key_id().size(), 0);
  EXPECT_GT(response->signed_jwt().size(), 0);
}

TEST(IamCredentialsIntegrationTest, SignJwtFailure) {
  std::string payload = R"({"some": "json"})";
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignJwt(
      "projects/-/serviceAccounts/"
      "bob@cloud-cpp-testing-resources.iam.gserviceaccount.com",
      {}, payload);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, GenerateAccessTokenProtoRequestSuccess) {
  ::google::iam::credentials::v1::GenerateAccessTokenRequest request;
  request.set_name(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com");
  *request.add_scope() = "https://www.googleapis.com/auth/spanner.admin";
  google::protobuf::Duration lifetime;
  lifetime.set_seconds(3600);
  *request.mutable_lifetime() = lifetime;
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateAccessToken(request);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->access_token().size(), 0);
}

TEST(IamCredentialsIntegrationTest, GenerateAccessTokenProtoRequestFailure) {
  ::google::iam::credentials::v1::GenerateAccessTokenRequest request;
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateAccessToken(request);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, GenerateIdTokenProtoRequestSuccess) {
  ::google::iam::credentials::v1::GenerateIdTokenRequest request;
  request.set_name(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com");
  request.set_audience("https://www.googleapis.com/auth/spanner.admin");
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateIdToken(request);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->token().size(), 0);
}

TEST(IamCredentialsIntegrationTest, GenerateIdTokenProtoRequestFailure) {
  ::google::iam::credentials::v1::GenerateIdTokenRequest request;
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.GenerateIdToken(request);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, SignBlobProtoRequestSuccess) {
  ::google::iam::credentials::v1::SignBlobRequest request;
  request.set_name(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com");
  request.set_payload("somebytes");
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignBlob(request);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->key_id().size(), 0);
  EXPECT_GT(response->signed_blob().size(), 0);
}

TEST(IamCredentialsIntegrationTest, SignBlobProtoRequestFailure) {
  ::google::iam::credentials::v1::SignBlobRequest request;
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignBlob(request);
  EXPECT_FALSE(response.status().ok());
}

TEST(IamCredentialsIntegrationTest, SignJwtProtoRequestSuccess) {
  ::google::iam::credentials::v1::SignJwtRequest request;
  request.set_name(
      "projects/-/serviceAccounts/"
      "kokoro-run@cloud-cpp-testing-resources.iam.gserviceaccount.com");
  request.set_payload(R"({"some": "json"})");
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignJwt(request);
  EXPECT_STATUS_OK(response);
  EXPECT_GT(response->key_id().size(), 0);
  EXPECT_GT(response->signed_jwt().size(), 0);
}

TEST(IamCredentialsIntegrationTest, SignJwtProtoRequestFailure) {
  ::google::iam::credentials::v1::SignJwtRequest request;
  auto client = IAMCredentialsClient(MakeIAMCredentialsConnection({}));
  auto response = client.SignJwt(request);
  EXPECT_FALSE(response.status().ok());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace iam
}  // namespace cloud
}  // namespace google
