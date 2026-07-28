// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "google/showcase/v1beta1/echo_client.h"
#include "google/showcase/v1beta1/internal/echo_connection_impl.h"
#include "google/showcase/v1beta1/internal/echo_metadata_decorator.h"
#include "google/showcase/v1beta1/internal/echo_option_defaults.h"
#include "google/showcase/v1beta1/internal/echo_stub.h"
#include "google/showcase/v1beta1/internal/echo_stub_factory.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace v1beta1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

class HeaderInterceptingEchoStub : public v1beta1_internal::EchoStub {
 public:
  HeaderInterceptingEchoStub(
      std::shared_ptr<v1beta1_internal::EchoStub> delegate,
      std::function<void(std::multimap<std::string, std::string> const&)>
          metadata_callback)
      : delegate_(std::move(delegate)),
        metadata_callback_(std::move(metadata_callback)) {}

  ~HeaderInterceptingEchoStub() override = default;

  StatusOr<google::showcase::v1beta1::EchoResponse> Echo(
      grpc::ClientContext& context, Options const& options,
      google::showcase::v1beta1::EchoRequest const& request) override {
    auto response = delegate_->Echo(context, options, request);
    ExtractMetadata(context);
    return response;
  }

  StatusOr<google::showcase::v1beta1::EchoErrorDetailsResponse>
  EchoErrorDetails(grpc::ClientContext& context, Options const& options,
                   google::showcase::v1beta1::EchoErrorDetailsRequest const&
                       request) override {
    return delegate_->EchoErrorDetails(context, options, request);
  }

  StatusOr<google::showcase::v1beta1::FailEchoWithDetailsResponse>
  FailEchoWithDetails(
      grpc::ClientContext& context, Options const& options,
      google::showcase::v1beta1::FailEchoWithDetailsRequest const& request)
      override {
    return delegate_->FailEchoWithDetails(context, options, request);
  }

  future<StatusOr<google::longrunning::Operation>> AsyncWait(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::showcase::v1beta1::WaitRequest const& request) override {
    return delegate_->AsyncWait(cq, std::move(context), std::move(options),
                                request);
  }

  StatusOr<google::longrunning::Operation> Wait(
      grpc::ClientContext& context, Options options,
      google::showcase::v1beta1::WaitRequest const& request) override {
    return delegate_->Wait(context, std::move(options), request);
  }

  StatusOr<google::showcase::v1beta1::BlockResponse> Block(
      grpc::ClientContext& context, Options const& options,
      google::showcase::v1beta1::BlockRequest const& request) override {
    return delegate_->Block(context, options, request);
  }

  StatusOr<google::cloud::location::ListLocationsResponse> ListLocations(
      grpc::ClientContext& context, Options const& options,
      google::cloud::location::ListLocationsRequest const& request) override {
    return delegate_->ListLocations(context, options, request);
  }

  StatusOr<google::cloud::location::Location> GetLocation(
      grpc::ClientContext& context, Options const& options,
      google::cloud::location::GetLocationRequest const& request) override {
    return delegate_->GetLocation(context, options, request);
  }

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::SetIamPolicyRequest const& request) override {
    return delegate_->SetIamPolicy(context, options, request);
  }

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::GetIamPolicyRequest const& request) override {
    return delegate_->GetIamPolicy(context, options, request);
  }

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::TestIamPermissionsRequest const& request) override {
    return delegate_->TestIamPermissions(context, options, request);
  }

  StatusOr<google::longrunning::ListOperationsResponse> ListOperations(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::ListOperationsRequest const& request) override {
    return delegate_->ListOperations(context, options, request);
  }

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::GetOperationRequest const& request) override {
    return delegate_->GetOperation(context, options, request);
  }

  Status DeleteOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::DeleteOperationRequest const& request) override {
    return delegate_->DeleteOperation(context, options, request);
  }

  Status CancelOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::CancelOperationRequest const& request) override {
    return delegate_->CancelOperation(context, options, request);
  }

  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::GetOperationRequest const& request) override {
    return delegate_->AsyncGetOperation(cq, std::move(context),
                                        std::move(options), request);
  }

  future<Status> AsyncCancelOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::CancelOperationRequest const& request) override {
    return delegate_->AsyncCancelOperation(cq, std::move(context),
                                           std::move(options), request);
  }

 private:
  void ExtractMetadata(grpc::ClientContext& context) {
    auto to_string = [](grpc::string_ref ref) {
      return ref.empty() ? std::string{} : std::string{ref.data(), ref.size()};
    };
    std::multimap<std::string, std::string> metadata;
    for (auto const& pair : context.GetServerInitialMetadata()) {
      metadata.emplace(to_string(pair.first), to_string(pair.second));
    }
    for (auto const& pair : context.GetServerTrailingMetadata()) {
      metadata.emplace(to_string(pair.first), to_string(pair.second));
    }
    metadata_callback_(metadata);
  }

  std::shared_ptr<v1beta1_internal::EchoStub> delegate_;
  std::function<void(std::multimap<std::string, std::string> const&)>
      metadata_callback_;
};

TEST(EchoGrpcIntegrationTest, EchoSuccessGrpcWithPqcVerification) {
  std::string ca_path;
  if (auto* ca_env = std::getenv("SHOWCASE_CA_CERT")) {
    ca_path = ca_env;
  } else {
    auto* test_srcdir = std::getenv("TEST_SRCDIR");
    ASSERT_THAT(test_srcdir, NotNull());
    ca_path = std::string(test_srcdir) + "/_main/ci/showcase/showcase.pem";
  }

  std::ifstream ca_file(ca_path);
  ASSERT_TRUE(ca_file.good()) << "Failed to open CA file at " << ca_path;

  std::string port = "7469";
  if (auto* port_env = std::getenv("SHOWCASE_PORT")) {
    port = port_env;
  }
  std::string endpoint = absl::StrCat("localhost:", port);

  auto credentials = MakeAccessTokenCredentials(
      "dummy-token", std::chrono::system_clock::now() + std::chrono::hours(1));

  auto options = Options{}
                     .set<EndpointOption>(endpoint)
                     .set<CARootsFilePathOption>(ca_path)
                     .set<UnifiedCredentialsOption>(credentials);
  options = v1beta1_internal::EchoDefaultOptions(std::move(options));

  auto background = internal::MakeBackgroundThreadsFactory(options)();
  auto auth = internal::CreateAuthenticationStrategy(background->cq(), options);
  auto real_stub = v1beta1_internal::CreateDefaultEchoStub(auth, options);

  std::multimap<std::string, std::string> intercepted_metadata;
  auto metadata_callback =
      [&intercepted_metadata](
          std::multimap<std::string, std::string> const& metadata) {
        intercepted_metadata = metadata;
      };

  std::shared_ptr<v1beta1_internal::EchoStub> stub =
      std::make_shared<HeaderInterceptingEchoStub>(
          std::move(real_stub), std::move(metadata_callback));

  stub = std::make_shared<v1beta1_internal::EchoMetadata>(
      std::move(stub), std::multimap<std::string, std::string>{});

  auto connection = std::make_shared<v1beta1_internal::EchoConnectionImpl>(
      std::move(background), std::move(stub), options);

  auto client = EchoClient(connection);

  ::google::showcase::v1beta1::EchoRequest request;
  request.set_content("Hello from C++ GAPIC gRPC!");

  auto response = client.Echo(request);
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(response->content(), Eq("Hello from C++ GAPIC gRPC!"));

  auto get_metadata_value =
      [](std::multimap<std::string, std::string> const& metadata,
         std::string const& key) -> std::string {
    for (auto const& pair : metadata) {
      if (absl::EqualsIgnoreCase(pair.first, key)) {
        return pair.second;
      }
    }
    return "";
  };

  std::string tls_group =
      get_metadata_value(intercepted_metadata, "x-showcase-tls-group");
  std::string supported_groups = get_metadata_value(
      intercepted_metadata, "x-showcase-tls-client-supported-groups");

  EXPECT_THAT(tls_group, Not(IsEmpty()))
      << "x-showcase-tls-group metadata not found";
  EXPECT_THAT(supported_groups, Not(IsEmpty()))
      << "x-showcase-tls-client-supported-groups metadata not found";

  // Assert PQC was used.
  EXPECT_THAT(tls_group, Eq("X25519MLKEM768"));
  EXPECT_THAT(supported_groups, HasSubstr("X25519MLKEM768"));
}

}  // namespace
}  // namespace v1beta1
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
