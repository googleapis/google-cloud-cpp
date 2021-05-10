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

#include "schema_admin_connection.h"
#include "google/cloud/pubsub/internal/default_retry_policies.h"
#include "google/cloud/pubsub/internal/schema_logging.h"
#include "google/cloud/pubsub/internal/schema_metadata.h"
#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using google::cloud::internal::Idempotency;
using google::cloud::internal::RetryLoop;

class SchemaAdminConnectionImpl : public pubsub::SchemaAdminConnection {
 public:
  explicit SchemaAdminConnectionImpl(
      std::shared_ptr<pubsub_internal::SchemaStub> stub,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : stub_(std::move(stub)),
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
  std::shared_ptr<pubsub_internal::SchemaStub> stub_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
};
}  // namespace

std::shared_ptr<pubsub::SchemaAdminConnection> MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options, std::shared_ptr<SchemaStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  stub = std::make_shared<SchemaMetadata>(std::move(stub));
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<SchemaLogging>(std::move(stub),
                                           options.tracing_options());
  }
  return std::make_shared<SchemaAdminConnectionImpl>(
      std::move(stub), std::move(retry_policy), std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal

namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SchemaAdminConnection::~SchemaAdminConnection() = default;

std::shared_ptr<SchemaAdminConnection> MakeSchemaAdminConnection(
    pubsub::ConnectionOptions const& options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto stub =
      pubsub_internal::CreateDefaultSchemaStub(options, /*channel_id=*/0);
  if (!retry_policy) retry_policy = pubsub_internal::DefaultRetryPolicy();
  if (!backoff_policy) backoff_policy = pubsub_internal::DefaultBackoffPolicy();
  return pubsub_internal::MakeSchemaAdminConnection(options, std::move(stub),
                                                    std::move(retry_policy),
                                                    std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
