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

#include "google/cloud/internal/grpc_impersonate_service_account.h"
#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/internal/unified_grpc_credentials.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::iam::credentials::v1::GenerateAccessTokenRequest;
using ::google::iam::credentials::v1::GenerateAccessTokenResponse;

AsyncAccessTokenSource MakeSource(ImpersonateServiceAccountConfig const& config,
                                  CompletionQueue cq, Options const& options) {
  auto stub = MakeMinimalIamCredentialsStub(
      CreateAuthenticationStrategy(config.base_credentials(), std::move(cq),
                                   options),
      MakeMinimalIamCredentialsOptions(options));

  GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/" +
                   config.target_service_account());
  *request.mutable_delegates() = {config.delegates().begin(),
                                  config.delegates().end()};
  *request.mutable_scope() = {config.scopes().begin(), config.scopes().end()};
  request.mutable_lifetime()->set_seconds(config.lifetime().count());

  return [stub, request](CompletionQueue& cq) {
    return stub
        ->AsyncGenerateAccessToken(cq, absl::make_unique<grpc::ClientContext>(),
                                   request)
        .then([](future<StatusOr<GenerateAccessTokenResponse>> f)
                  -> StatusOr<AccessToken> {
          auto response = f.get();
          if (!response) return std::move(response).status();
          auto expiration = ToChronoTimePoint(response->expire_time());
          return AccessToken{std::move(*response->mutable_access_token()),
                             expiration};
        });
  };
}

std::shared_ptr<GrpcAsyncAccessTokenCache> MakeCache(
    CompletionQueue cq, ImpersonateServiceAccountConfig const& config,
    Options const& options) {
  auto source = MakeSource(config, cq, options);
  return GrpcAsyncAccessTokenCache::Create(std::move(cq), std::move(source));
}

}  // namespace

std::shared_ptr<GrpcImpersonateServiceAccount>
GrpcImpersonateServiceAccount::Create(
    CompletionQueue cq, ImpersonateServiceAccountConfig const& config,
    Options const& options) {
  return std::shared_ptr<GrpcImpersonateServiceAccount>(
      new GrpcImpersonateServiceAccount(std::move(cq), config, options));
}

GrpcImpersonateServiceAccount::GrpcImpersonateServiceAccount(
    CompletionQueue cq, ImpersonateServiceAccountConfig const& config,
    Options const& opts)
    : cache_(MakeCache(std::move(cq), config, opts)) {
  auto cainfo = LoadCAInfo(opts);
  if (cainfo) ssl_options_.pem_root_certs = std::move(*cainfo);
}

GrpcImpersonateServiceAccount::~GrpcImpersonateServiceAccount() = default;

std::shared_ptr<grpc::Channel> GrpcImpersonateServiceAccount::CreateChannel(
    std::string const& endpoint, grpc::ChannelArguments const& arguments) {
  auto credentials = grpc::SslCredentials(ssl_options_);
  return grpc::CreateCustomChannel(endpoint, credentials, arguments);
}

bool GrpcImpersonateServiceAccount::RequiresConfigureContext() const {
  return true;
}

Status GrpcImpersonateServiceAccount::ConfigureContext(
    grpc::ClientContext& context) {
  auto token = cache_->GetAccessToken();
  if (!token) return std::move(token).status();
  context.set_credentials(UpdateCallCredentials(std::move(token->token)));
  return Status{};
}

future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
GrpcImpersonateServiceAccount::AsyncConfigureContext(
    std::unique_ptr<grpc::ClientContext> context) {
  struct Capture {
    std::weak_ptr<GrpcImpersonateServiceAccount> w;
    std::unique_ptr<grpc::ClientContext> context;

    StatusOr<std::unique_ptr<grpc::ClientContext>> operator()(
        future<StatusOr<AccessToken>> f) {
      auto self = w.lock();
      if (!self) return Status{StatusCode::kUnknown, "lost reference"};
      return self->OnGetCallCredentials(std::move(context), f.get());
    }
  };
  return cache_->AsyncGetAccessToken().then(
      Capture{WeakFromThis(), std::move(context)});
}

std::shared_ptr<grpc::CallCredentials>
GrpcImpersonateServiceAccount::UpdateCallCredentials(std::string token) {
  std::lock_guard<std::mutex> lk(mu_);
  if (access_token_ != token) {
    credentials_ = grpc::AccessTokenCredentials(token);
    access_token_ = std::move(token);
  }
  return credentials_;
}

StatusOr<std::unique_ptr<grpc::ClientContext>>
GrpcImpersonateServiceAccount::OnGetCallCredentials(
    std::unique_ptr<grpc::ClientContext> context,
    StatusOr<AccessToken> result) {
  if (!result) return std::move(result).status();
  context->set_credentials(UpdateCallCredentials(std::move(result->token)));
  return make_status_or(std::move(context));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
