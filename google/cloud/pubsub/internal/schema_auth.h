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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SCHEMA_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SCHEMA_AUTH_H

#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A Decorator for `SchemaStub` that logs each request and response.
 */
class SchemaAuth : public SchemaStub {
 public:
  SchemaAuth(
      std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth,
      std::shared_ptr<SchemaStub> child)
      : auth_(std::move(auth)), child_(std::move(child)) {}

  StatusOr<google::pubsub::v1::Schema> CreateSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::CreateSchemaRequest const& request) override;
  StatusOr<google::pubsub::v1::Schema> GetSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSchemaRequest const& request) override;
  StatusOr<google::pubsub::v1::ListSchemasResponse> ListSchemas(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSchemasRequest const& request) override;
  Status DeleteSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSchemaRequest const& request) override;
  StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::ValidateSchemaRequest const& request) override;
  StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessage(
      grpc::ClientContext& context,
      google::pubsub::v1::ValidateMessageRequest const& request) override;

 private:
  std::shared_ptr<google::cloud::internal::GrpcAuthenticationStrategy> auth_;
  std::shared_ptr<SchemaStub> child_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SCHEMA_AUTH_H
