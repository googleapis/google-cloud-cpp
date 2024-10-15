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
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/internal/trace_propagator.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/internal/url_encode.h"
#include <google/iam/credentials/v1/iamcredentials.grpc.pb.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
      CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    using ResultType = StatusOr<GenerateAccessTokenResponse>;
    auto& impl = impl_;
    auto async_call = [impl](grpc::ClientContext* context,
                             GenerateAccessTokenRequest const& request,
                             grpc::CompletionQueue* cq) {
      return impl->AsyncGenerateAccessToken(context, request, cq);
    };
    return auth_strategy_->AsyncConfigureContext(std::move(context))
        .then([cq, async_call,
               request](future<StatusOr<std::shared_ptr<grpc::ClientContext>>>
                            f) mutable -> future<ResultType> {
          auto context = f.get();
          if (!context)
            return make_ready_future(ResultType(std::move(context).status()));
          return MakeUnaryRpcImpl<GenerateAccessTokenRequest,
                                  GenerateAccessTokenResponse>(
              cq, async_call, request, *std::move(context));
        });
  }

  StatusOr<google::iam::credentials::v1::SignBlobResponse> SignBlob(
      grpc::ClientContext& context,
      google::iam::credentials::v1::SignBlobRequest const& request) override {
    auto status = auth_strategy_->ConfigureContext(context);
    if (!status.ok()) return status;
    google::iam::credentials::v1::SignBlobResponse response;
    auto grpc = impl_->SignBlob(&context, request, &response);
    if (!grpc.ok()) return MakeStatusFromRpcError(grpc);
    return response;
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
        x_goog_api_client_(
            google::cloud::internal::HandCraftedLibClientHeader()) {}
  ~AsyncAccessTokenGeneratorMetadata() override = default;

  future<StatusOr<GenerateAccessTokenResponse>> AsyncGenerateAccessToken(
      CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    context->AddMetadata("x-goog-request-params",
                         "name=" + internal::UrlEncode(request.name()));
    context->AddMetadata("x-goog-api-client", x_goog_api_client_);
    return child_->AsyncGenerateAccessToken(cq, std::move(context), request);
  }

  StatusOr<google::iam::credentials::v1::SignBlobResponse> SignBlob(
      grpc::ClientContext& context,
      google::iam::credentials::v1::SignBlobRequest const& request) override {
    context.AddMetadata("x-goog-request-params",
                        "name=" + internal::UrlEncode(request.name()));
    context.AddMetadata("x-goog-api-client", x_goog_api_client_);
    return child_->SignBlob(context, request);
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
      CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    auto prefix = std::string(__func__) + "(" + RequestIdForLogging() + ")";
    auto const& opts = tracing_options_;
    GCP_LOG(DEBUG) << prefix << " << " << DebugString(request, opts);
    return child_->AsyncGenerateAccessToken(cq, std::move(context), request)
        .then([prefix, opts](future<StatusOr<GenerateAccessTokenResponse>> f) {
          auto response = f.get();
          if (!response) {
            GCP_LOG(DEBUG) << prefix << " >> status="
                           << DebugString(response.status(), opts);
          } else {
            // We do not want to log the access token
            GCP_LOG(DEBUG) << prefix
                           << " >> response={access_token=[censored]}";
          }
          return response;
        });
  }

  StatusOr<google::iam::credentials::v1::SignBlobResponse> SignBlob(
      grpc::ClientContext& context,
      google::iam::credentials::v1::SignBlobRequest const& request) override {
    return google::cloud::internal::LogWrapper(
        [this](grpc::ClientContext& context,
               google::iam::credentials::v1::SignBlobRequest const& request) {
          return child_->SignBlob(context, request);
        },
        context, request, __func__, tracing_options_);
  }

 private:
  std::shared_ptr<MinimalIamCredentialsStub> child_;
  TracingOptions tracing_options_;
};

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
class AsyncAccessTokenGeneratorTracing : public MinimalIamCredentialsStub {
 public:
  explicit AsyncAccessTokenGeneratorTracing(
      std::shared_ptr<MinimalIamCredentialsStub> child)
      : child_(std::move(child)), propagator_(MakePropagator()) {}
  ~AsyncAccessTokenGeneratorTracing() override = default;

  future<StatusOr<GenerateAccessTokenResponse>> AsyncGenerateAccessToken(
      CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
      GenerateAccessTokenRequest const& request) override {
    auto span = MakeSpanGrpc("google.iam.credentials.v1.IAMCredentials",
                             "GenerateAccessToken");
    {
      auto scope = opentelemetry::trace::Scope(span);
      InjectTraceContext(*context, *propagator_);
    }
    auto f = child_->AsyncGenerateAccessToken(cq, context, request);
    return EndSpan(std::move(context), std::move(span), std::move(f));
  }

  StatusOr<google::iam::credentials::v1::SignBlobResponse> SignBlob(
      grpc::ClientContext& context,
      google::iam::credentials::v1::SignBlobRequest const& request) override {
    auto span =
        MakeSpanGrpc("google.iam.credentials.v1.IAMCredentials", "SignBlob");
    auto scope = opentelemetry::trace::Scope(span);
    InjectTraceContext(context, *propagator_);
    return EndSpan(context, *span, child_->SignBlob(context, request));
  }

 private:
  std::shared_ptr<MinimalIamCredentialsStub> child_;
  std::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>
      propagator_;
};
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace

std::shared_ptr<MinimalIamCredentialsStub> DecorateMinimalIamCredentialsStub(
    std::shared_ptr<MinimalIamCredentialsStub> impl, Options const& options) {
  impl = std::make_shared<AsyncAccessTokenGeneratorMetadata>(std::move(impl));
  auto const& components = options.get<LoggingComponentsOption>();
  if (Contains(components, "auth") || Contains(components, "rpc")) {
    impl = std::make_shared<AsyncAccessTokenGeneratorLogging>(
        std::move(impl), options.get<GrpcTracingOptionsOption>());
  }
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (TracingEnabled(options)) {
    impl = std::make_shared<AsyncAccessTokenGeneratorTracing>(std::move(impl));
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return impl;
}

std::shared_ptr<MinimalIamCredentialsStub> MakeMinimalIamCredentialsStub(
    std::shared_ptr<GrpcAuthenticationStrategy> auth_strategy,
    Options const& options) {
  auto channel = auth_strategy->CreateChannel(options.get<EndpointOption>(),
                                              grpc::ChannelArguments{});
  std::cout << "endpoint: " << options.get<EndpointOption>() << std::endl;
  auto impl = std::make_shared<MinimalIamCredentialsImpl>(
      std::move(auth_strategy), IAMCredentials::NewStub(std::move(channel)));

  return DecorateMinimalIamCredentialsStub(std::move(impl), options);
}

Options MakeMinimalIamCredentialsOptions(Options options) {
  // The supplied options come from a service. We are overriding the value of
  // its `EndpointOption`.
  options.unset<EndpointOption>();
  auto ep = UniverseDomainEndpoint("iamcredentials.googleapis.com", options);
  return options.set<EndpointOption>(std::move(ep));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
