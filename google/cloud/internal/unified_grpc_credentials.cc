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

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

class GrpcChannelCredentialsAuthentication : public GrpcAuthenticationStrategy {
 public:
  explicit GrpcChannelCredentialsAuthentication(
      std::shared_ptr<grpc::ChannelCredentials> c)
      : credentials_(std::move(c)) {}
  ~GrpcChannelCredentialsAuthentication() override = default;

  std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const& endpoint,
      grpc::ChannelArguments const& arguments) override {
    return grpc::CreateCustomChannel(endpoint, credentials_, arguments);
  }
  Status Setup(grpc::ClientContext&) override { return Status{}; }

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
};

auto constexpr kExpirationSlack = std::chrono::minutes(5);

class DynamicAccessTokenAuthenticationStrategy
    : public GrpcAuthenticationStrategy {
 public:
  explicit DynamicAccessTokenAuthenticationStrategy(AccessTokenSource source)
      : source_(std::move(source)) {}

  std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const& endpoint,
      grpc::ChannelArguments const& arguments) override {
    auto credentials = grpc::SslCredentials(grpc::SslCredentialsOptions{});
    return grpc::CreateCustomChannel(endpoint, credentials, arguments);
  }
  Status Setup(grpc::ClientContext& context) override {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return !refreshing_; });
    auto const deadline = std::chrono::system_clock::now() + kExpirationSlack;
    if (deadline < expiration_) {
      context.set_credentials(credentials_);
      return {};
    }
    refreshing_ = true;
    lk.unlock();
    auto refresh = source_();
    lk.lock();
    refreshing_ = false;
    token_ = std::move(refresh.token);
    expiration_ = refresh.expiration;
    credentials_ = grpc::AccessTokenCredentials(token_);
    context.set_credentials(credentials_);
    cv_.notify_all();
    return Status{};
  }

 private:
  AccessTokenSource source_;
  std::mutex mu_;
  std::string token_;
  std::shared_ptr<grpc::CallCredentials> credentials_;
  std::chrono::system_clock::time_point expiration_;
  bool refreshing_ = false;
  std::condition_variable cv_;
};

}  // namespace

std::unique_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<Credentials> const& credentials) {
  struct Visitor : public CredentialsVisitor {
    std::unique_ptr<GrpcAuthenticationStrategy> result;

    void visit(GoogleDefaultCredentialsConfig&) override {
      result = absl::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::GoogleDefaultCredentials());
    }
    void visit(DynamicAccessTokenConfig& cfg) override {
      result = absl::make_unique<DynamicAccessTokenAuthenticationStrategy>(
          cfg.source());
    }
  } visitor;

  CredentialsVisitor::dispatch(*credentials, visitor);
  return std::move(visitor.result);
}

std::unique_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<grpc::ChannelCredentials> const& credentials) {
  return absl::make_unique<GrpcChannelCredentialsAuthentication>(credentials);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
