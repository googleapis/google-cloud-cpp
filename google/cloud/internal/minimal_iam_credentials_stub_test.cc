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

#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/mock_minimal_iam_credentials_stub.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <google/iam/credentials/v1/iamcredentials.grpc.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockMinimalIamCredentialsStub;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::iam::credentials::v1::GenerateAccessTokenRequest;
using ::google::iam::credentials::v1::GenerateAccessTokenResponse;
using ::google::iam::credentials::v1::SignBlobRequest;
using ::google::iam::credentials::v1::SignBlobResponse;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class MinimalIamCredentialsStubTest : public ::testing::Test {
 protected:
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request, google::cloud::internal::ApiClientHeader());
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(MinimalIamCredentialsStubTest, AsyncGenerateAccessTokenLogging) {
  auto mock = std::make_shared<MockMinimalIamCredentialsStub>();
  EXPECT_CALL(*mock, AsyncGenerateAccessToken)
      .WillOnce([](CompletionQueue&, auto, GenerateAccessTokenRequest const&) {
        GenerateAccessTokenResponse response;
        response.set_access_token("test-only-token");
        return make_ready_future(make_status_or(response));
      });
  auto stub = DecorateMinimalIamCredentialsStub(
      mock, Options{}
                .set<TracingComponentsOption>({"rpc"})
                .set<GrpcTracingOptionsOption>(
                    TracingOptions{}.SetOptions("single_line_mode")));
  GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/test-only-sa@not-valid");
  CompletionQueue cq;
  auto response = stub->AsyncGenerateAccessToken(
                          cq, std::make_shared<grpc::ClientContext>(), request)
                      .get();
  ASSERT_THAT(response, IsOk());
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Contains(HasSubstr("AsyncGenerateAccessToken")));
  EXPECT_THAT(lines, Contains(HasSubstr("[censored]")));
  EXPECT_THAT(lines, Not(Contains(HasSubstr("test-only-token"))));
}

TEST_F(MinimalIamCredentialsStubTest, AsyncGenerateAccessTokenNoLogging) {
  auto mock = std::make_shared<MockMinimalIamCredentialsStub>();
  EXPECT_CALL(*mock, AsyncGenerateAccessToken)
      .WillOnce([](CompletionQueue&, auto, GenerateAccessTokenRequest const&) {
        GenerateAccessTokenResponse response;
        response.set_access_token("test-only-token");
        return make_ready_future(make_status_or(response));
      });
  auto stub = DecorateMinimalIamCredentialsStub(
      mock, Options{}.set<TracingComponentsOption>({}));
  GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/test-only-sa@not-valid");
  CompletionQueue cq;
  auto response = stub->AsyncGenerateAccessToken(
                          cq, std::make_shared<grpc::ClientContext>(), request)
                      .get();
  ASSERT_THAT(response, IsOk());
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Not(Contains(HasSubstr("AsyncGenerateAccessToken"))));
}

TEST_F(MinimalIamCredentialsStubTest, SignBlobLogging) {
  auto mock = std::make_shared<MockMinimalIamCredentialsStub>();
  SignBlobResponse expected;
  expected.set_signed_blob("test-only-signed");
  EXPECT_CALL(*mock, SignBlob).WillOnce(Return(expected));
  auto stub = DecorateMinimalIamCredentialsStub(
      mock, Options{}
                .set<TracingComponentsOption>({"rpc"})
                .set<GrpcTracingOptionsOption>(
                    TracingOptions{}.SetOptions("single_line_mode")));
  SignBlobRequest request;
  request.set_name("projects/-/serviceAccounts/test-only-sa@not-valid");
  grpc::ClientContext context;
  auto response = stub->SignBlob(context, request);
  ASSERT_THAT(response, IsOk());
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Contains(HasSubstr("SignBlob")));
  EXPECT_THAT(lines, Contains(HasSubstr("test-only-signed")));
}

TEST_F(MinimalIamCredentialsStubTest, SignBlobNoLogging) {
  auto mock = std::make_shared<MockMinimalIamCredentialsStub>();
  SignBlobResponse expected;
  expected.set_signed_blob("test-only-signed");
  EXPECT_CALL(*mock, SignBlob).WillOnce(Return(expected));
  auto stub = DecorateMinimalIamCredentialsStub(
      mock, Options{}.set<TracingComponentsOption>({}));
  SignBlobRequest request;
  request.set_name("projects/-/serviceAccounts/test-only-sa@not-valid");
  grpc::ClientContext context;
  auto response = stub->SignBlob(context, request);
  ASSERT_THAT(response, IsOk());
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Not(Contains(HasSubstr("SignBlob"))));
}

TEST_F(MinimalIamCredentialsStubTest, Invalid) {
  auto stub = MakeMinimalIamCredentialsStub(
      CreateAuthenticationStrategy(grpc::InsecureChannelCredentials()),
      Options{}.set<EndpointOption>("localhost:1"));

  GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/test-only-sa@not-valid");
  AutomaticallyCreatedBackgroundThreads background;
  auto cq = background.cq();
  auto response = stub->AsyncGenerateAccessToken(
                          cq, std::make_shared<grpc::ClientContext>(), request)
                      .get();
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

// For some reasons the method name is not found in IsContextMDValid()
TEST_F(MinimalIamCredentialsStubTest,
       DISABLED_AsyncGenerateAccessTokenMetadata) {
  auto mock = std::make_shared<MockMinimalIamCredentialsStub>();
  EXPECT_CALL(*mock, AsyncGenerateAccessToken)
      .WillOnce([this](CompletionQueue&, auto context,
                       GenerateAccessTokenRequest const& request) {
        IsContextMDValid(
            *context,
            "google.iam.credentials.v1.IAMCredentials.GenerateAccessToken",
            request);
        GenerateAccessTokenResponse response;
        response.set_access_token("test-only-token");
        return make_ready_future(make_status_or(response));
      });

  auto stub = DecorateMinimalIamCredentialsStub(
      mock, Options{}.set<TracingComponentsOption>({}));
  GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/test-only-sa@not-valid");
  CompletionQueue cq;
  auto response = stub->AsyncGenerateAccessToken(
                          cq, std::make_shared<grpc::ClientContext>(), request)
                      .get();
  ASSERT_THAT(response, IsOk());
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Not(Contains(HasSubstr("AsyncGenerateAccessToken"))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
