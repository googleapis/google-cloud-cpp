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

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/grpc_access_token_authentication.h"
#include "google/cloud/internal/grpc_channel_credentials_authentication.h"
#include "google/cloud/internal/grpc_impersonate_service_account.h"
#include "google/cloud/internal/grpc_service_account_authentication.h"
#include "absl/memory/memory.h"
#include <grpcpp/security/credentials.h>
#include <fstream>

namespace {

// Put this outside our own namespace to avoid conflicts when using a naked
// `ExternalAccountCredentials()` call.
std::shared_ptr<grpc::CallCredentials> GrpcExternalAccountCredentials(
    google::cloud::internal::ExternalAccountConfig const& cfg) {
  // The using directives are a deviation from the Google Style Guide. The
  // `ExternalAccountCredentials()` function appeared in gRPC 1.35, in the
  // `grpc::experimental` namespace. It was moved to the `grpc` namespace in
  // gRPC 1.36. We support gRPC >= 1.35. gRPC does not offer version macros
  // until gRPC 1.51. We think the minor deviation from the style guide is
  // justified in this case.
  using namespace ::grpc;                // NOLINT(google-build-using-namespace)
  using namespace ::grpc::experimental;  // NOLINT(google-build-using-namespace)
  return ExternalAccountCredentials(
      cfg.json_object(), cfg.options().get<google::cloud::ScopesOption>());
}

}  // namespace

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    google::cloud::CompletionQueue cq, Options const& options) {
  if (options.has<google::cloud::UnifiedCredentialsOption>()) {
    return google::cloud::internal::CreateAuthenticationStrategy(
        options.get<google::cloud::UnifiedCredentialsOption>(), std::move(cq),
        options);
  }
  return google::cloud::internal::CreateAuthenticationStrategy(
      options.get<google::cloud::GrpcCredentialOption>());
}

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<Credentials> const& credentials, CompletionQueue cq,
    Options options) {
  struct Visitor : public CredentialsVisitor {
    CompletionQueue cq;
    Options options;
    std::shared_ptr<GrpcAuthenticationStrategy> result;

    Visitor(CompletionQueue c, Options o)
        : cq(std::move(c)), options(std::move(o)) {}

    void visit(InsecureCredentialsConfig&) override {
      result = absl::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::InsecureChannelCredentials());
    }
    void visit(GoogleDefaultCredentialsConfig&) override {
      result = absl::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::GoogleDefaultCredentials());
    }
    void visit(AccessTokenConfig& cfg) override {
      result = absl::make_unique<GrpcAccessTokenAuthentication>(
          cfg.access_token(), std::move(options));
    }
    void visit(ImpersonateServiceAccountConfig& cfg) override {
      result = GrpcImpersonateServiceAccount::Create(std::move(cq), cfg,
                                                     std::move(options));
    }
    void visit(ServiceAccountConfig& cfg) override {
      result = absl::make_unique<GrpcServiceAccountAuthentication>(
          cfg.json_object(), std::move(options));
    }
    void visit(ExternalAccountConfig& cfg) override {
      result = absl::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::CompositeChannelCredentials(
              grpc::SslCredentials(grpc::SslCredentialsOptions()),
              GrpcExternalAccountCredentials(cfg)));
    }
  } visitor(std::move(cq), std::move(options));

  CredentialsVisitor::dispatch(*credentials, visitor);
  return std::move(visitor.result);
}

std::shared_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<grpc::ChannelCredentials> const& credentials) {
  return std::make_shared<GrpcChannelCredentialsAuthentication>(credentials);
}

absl::optional<std::string> LoadCAInfo(Options const& opts) {
  if (!opts.has<CARootsFilePathOption>()) return absl::nullopt;
  std::ifstream is(opts.get<CARootsFilePathOption>());
  return std::string{std::istreambuf_iterator<char>{is.rdbuf()}, {}};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
