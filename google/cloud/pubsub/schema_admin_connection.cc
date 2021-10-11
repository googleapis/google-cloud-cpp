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

#include "google/cloud/pubsub/schema_admin_connection.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/schema_auth.h"
#include "google/cloud/pubsub/internal/schema_logging.h"
#include "google/cloud/pubsub/internal/schema_metadata.h"
#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

using ::google::cloud::internal::Idempotency;
using ::google::cloud::internal::RetryLoop;

class SchemaAdminConnectionImpl : public pubsub::SchemaAdminConnection {
 public:
  explicit SchemaAdminConnectionImpl(
      std::unique_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<pubsub_internal::SchemaStub> stub,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : background_(std::move(background)),
        stub_(std::move(stub)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  ~SchemaAdminConnectionImpl() override = default;

  StatusOr<google::pubsub::v1::Schema> CreateSchema(
      google::pubsub::v1::CreateSchemaRequest const& request) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::CreateSchemaRequest const& request) {
          return stub_->CreateSchema(context, request);
        },
        request, __func__);
  }
  StatusOr<google::pubsub::v1::Schema> GetSchema(
      google::pubsub::v1::GetSchemaRequest const& request) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::GetSchemaRequest const& request) {
          return stub_->GetSchema(context, request);
        },
        request, __func__);
  }

  pubsub::ListSchemasRange ListSchemas(
      google::pubsub::v1::ListSchemasRequest const& request) override {
    auto& stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<pubsub::RetryPolicy const>(retry_policy_->clone());
    auto backoff =
        std::shared_ptr<pubsub::BackoffPolicy const>(backoff_policy_->clone());
    char const* function_name = __func__;
    auto list_functor =
        [stub, retry, backoff,
         function_name](google::pubsub::v1::ListSchemasRequest const& request) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& c,
                     google::pubsub::v1::ListSchemasRequest const& r) {
                return stub->ListSchemas(c, r);
              },
              request, function_name);
        };
    return internal::MakePaginationRange<pubsub::ListSchemasRange>(
        std::move(request), list_functor,
        [](google::pubsub::v1::ListSchemasResponse response) {
          std::vector<google::pubsub::v1::Schema> items;
          items.reserve(response.schemas_size());
          for (auto& item : *response.mutable_schemas()) {
            items.push_back(std::move(item));
          }
          return items;
        });
  }

  Status DeleteSchema(
      google::pubsub::v1::DeleteSchemaRequest const& request) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::DeleteSchemaRequest const& request) {
          return stub_->DeleteSchema(context, request);
        },
        request, __func__);
  }
  StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateSchema(
      google::pubsub::v1::ValidateSchemaRequest const& request) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::ValidateSchemaRequest const& request) {
          return stub_->ValidateSchema(context, request);
        },
        request, __func__);
  }
  StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessage(
      google::pubsub::v1::ValidateMessageRequest const& request) override {
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::pubsub::v1::ValidateMessageRequest const& request) {
          return stub_->ValidateMessage(context, request);
        },
        request, __func__);
  }

 private:
  std::unique_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<pubsub_internal::SchemaStub> stub_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
};

// Decorates a SchemaAdminStub. This works for both mock and real stubs.
std::shared_ptr<pubsub_internal::SchemaStub> DecorateSchemaAdminStub(
    Options const& opts,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    std::shared_ptr<SchemaStub> stub) {
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<pubsub_internal::SchemaAuth>(std::move(auth),
                                                         std::move(stub));
  }
  stub = std::make_shared<pubsub_internal::SchemaMetadata>(std::move(stub));
  if (internal::Contains(opts.get<TracingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::SchemaLogging>(
        std::move(stub), opts.get<GrpcTracingOptionsOption>());
  }
  return stub;
}

}  // namespace

std::shared_ptr<pubsub::SchemaAdminConnection> MakeSchemaAdminConnection(
    Options const& opts, std::shared_ptr<SchemaStub> stub) {
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      google::cloud::MakeInsecureCredentials(), background->cq(), opts);
  stub = DecorateSchemaAdminStub(opts, std::move(auth), std::move(stub));
  return std::make_shared<SchemaAdminConnectionImpl>(
      std::move(background), std::move(stub),
      opts.get<pubsub::RetryPolicyOption>()->clone(),
      opts.get<pubsub::BackoffPolicyOption>()->clone());
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace pubsub_internal

namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_NS {

SchemaAdminConnection::~SchemaAdminConnection() = default;

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(
    std::initializer_list<pubsub_internal::NonConstructible>) {
  return MakeSchemaAdminConnection();
}

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 PolicyOptionList>(opts, __func__);
  opts = pubsub_internal::DefaultCommonOptions(std::move(opts));

  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      background->cq(), opts);

  auto stub = pubsub_internal::CreateDefaultSchemaStub(auth->CreateChannel(
      opts.get<EndpointOption>(), internal::MakeChannelArguments(opts)));

  stub = pubsub_internal::DecorateSchemaAdminStub(opts, std::move(auth),
                                                  std::move(stub));
  return std::make_shared<pubsub_internal::SchemaAdminConnectionImpl>(
      std::move(background), std::move(stub),
      opts.get<pubsub::RetryPolicyOption>()->clone(),
      opts.get<pubsub::BackoffPolicyOption>()->clone());
}

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto opts = internal::MakeOptions(options);
  if (retry_policy) opts.set<RetryPolicyOption>(retry_policy->clone());
  if (backoff_policy) opts.set<BackoffPolicyOption>(backoff_policy->clone());
  return MakeSchemaAdminConnection(std::move(opts));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
