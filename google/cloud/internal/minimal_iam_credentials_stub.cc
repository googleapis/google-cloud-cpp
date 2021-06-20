// Copyright 2021 Google LLC
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

#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <google/iam/credentials/v1/iamcredentials.grpc.pb.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::iam::credentials::v1::GenerateAccessTokenRequest;
using ::google::iam::credentials::v1::GenerateAccessTokenResponse;
using ::google::iam::credentials::v1::IAMCredentials;

class MinimalIamCredentialsImpl : public MinimalIamCredentialsStub {
 public:
  explicit MinimalIamCredentialsImpl(
      std::shared_ptr<GrpcAuthenticationStrategy> auth_strategy,
      std::shared_ptr<IAMCredentials::StubInterface> impl)
      : auth_strategy_(std::move(auth_strategy)), impl_(std::move(impl)) {}
  ~MinimalIamCredentialsImpl() override = default;

  future<StatusOr<GenerateAccessTokenResponse>> AsyncGenerateAccessToken(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    using ResultType = StatusOr<GenerateAccessTokenResponse>;
    auto impl = impl_;
    auto async_call = [impl](grpc::ClientContext* context,
                             GenerateAccessTokenRequest const& request,
                             grpc::CompletionQueue* cq) {
      return impl->AsyncGenerateAccessToken(context, request, cq);
    };
    return auth_strategy_->AsyncConfigureContext(std::move(context))
        .then([cq, async_call,
               request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                            f) mutable -> future<ResultType> {
          auto context = f.get();
          if (!context)
            return make_ready_future(ResultType(std::move(context).status()));
          return cq.MakeUnaryRpc(async_call, request, *std::move(context));
        });
  }

 private:
  std::shared_ptr<GrpcAuthenticationStrategy> auth_strategy_;
  std::shared_ptr<IAMCredentials::StubInterface> impl_;
};

class AsyncAccessTokenGeneratorMetadata : public MinimalIamCredentialsStub {
 public:
  explicit AsyncAccessTokenGeneratorMetadata(
      std::shared_ptr<MinimalIamCredentialsStub> child)
      : child_(std::move(child)),
        x_goog_api_client_(google::cloud::internal::ApiClientHeader()) {}
  ~AsyncAccessTokenGeneratorMetadata() override = default;

  future<StatusOr<GenerateAccessTokenResponse>> AsyncGenerateAccessToken(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    context->AddMetadata("x-goog-request-params", "name=" + request.name());
    context->AddMetadata("x-goog-api-client", x_goog_api_client_);
    return child_->AsyncGenerateAccessToken(cq, std::move(context), request);
  }

 private:
  std::shared_ptr<MinimalIamCredentialsStub> child_;
  std::string x_goog_api_client_;
};

class AsyncAccessTokenGeneratorLogging : public MinimalIamCredentialsStub {
 public:
  AsyncAccessTokenGeneratorLogging(
      std::shared_ptr<MinimalIamCredentialsStub> child,
      TracingOptions tracing_options)
      : child_(std::move(child)),
        tracing_options_(std::move(tracing_options)) {}
  ~AsyncAccessTokenGeneratorLogging() override = default;

  future<StatusOr<GenerateAccessTokenResponse>> AsyncGenerateAccessToken(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    auto prefix = std::string(__func__) + "(" + RequestIdForLogging() + ")";
    GCP_LOG(DEBUG) << prefix << " << "
                   << DebugString(request, tracing_options_);
    return child_->AsyncGenerateAccessToken(cq, std::move(context), request)
        .then([prefix](future<StatusOr<GenerateAccessTokenResponse>> f) {
          auto response = f.get();
          if (!response) {
            GCP_LOG(DEBUG) << prefix << " >> status=" << response.status();
          } else {
            // We do not want to log the access token
            GCP_LOG(DEBUG) << prefix
                           << " >> response={access_token=[censored]}";
          }
          return response;
        });
  }

 private:
  std::shared_ptr<MinimalIamCredentialsStub> child_;
  TracingOptions tracing_options_;
};

}  // namespace

std::shared_ptr<MinimalIamCredentialsStub> DecorateMinimalIamCredentialsStub(
    std::shared_ptr<MinimalIamCredentialsStub> impl, Options const& options) {
  impl = std::make_shared<AsyncAccessTokenGeneratorMetadata>(std::move(impl));
  if (Contains(options.get<TracingComponentsOption>(), "rpc")) {
    impl = std::make_shared<AsyncAccessTokenGeneratorLogging>(
        std::move(impl), options.get<GrpcTracingOptionsOption>());
  }
  return impl;
}

std::shared_ptr<MinimalIamCredentialsStub> MakeMinimalIamCredentialsStub(
    std::shared_ptr<GrpcAuthenticationStrategy> auth_strategy,
    Options const& options) {
  auto channel = auth_strategy->CreateChannel(options.get<EndpointOption>(),
                                              grpc::ChannelArguments{});
  auto impl = std::make_shared<MinimalIamCredentialsImpl>(
      std::move(auth_strategy), IAMCredentials::NewStub(std::move(channel)));

  return DecorateMinimalIamCredentialsStub(std::move(impl), options);
}

Options MakeMinimalIamCredentialsOptions(Options options) {
  return MergeOptions(
      Options{}.set<EndpointOption>("iamcredentials.googleapis.com"),
      std::move(options));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
