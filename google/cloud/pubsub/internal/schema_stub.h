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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SCHEMA_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SCHEMA_STUB_H

#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include <google/pubsub/v1/schema.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Define the interface for the gRPC wrapper.
 *
 * We wrap the gRPC-generated `SchemaStub` to:
 *   - Return a `StatusOr<T>` instead of using a `grpc::Status` and an "output
 *     parameter" for the response.
 *   - To be able to mock the stubs.
 *   - To be able to decompose some functionality (logging, adding metadata
 *     information) into layers.
 */
class SchemaStub {
 public:
  virtual ~SchemaStub() = default;

  /// Creates a schema.
  virtual StatusOr<google::pubsub::v1::Schema> CreateSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::CreateSchemaRequest const& request) = 0;

  /// Gets a schema.
  virtual StatusOr<google::pubsub::v1::Schema> GetSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSchemaRequest const& request) = 0;

  /// Lists schemas in a project.
  virtual StatusOr<google::pubsub::v1::ListSchemasResponse> ListSchemas(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSchemasRequest const& request) = 0;

  /// Deletes a schema.
  virtual Status DeleteSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSchemaRequest const& request) = 0;

  /// Validates a schema.
  virtual StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::ValidateSchemaRequest const& request) = 0;

  /// Validates a message against a schema.
  virtual StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessage(
      grpc::ClientContext& context,
      google::pubsub::v1::ValidateMessageRequest const& request) = 0;
};

class DefaultSchemaStub : public SchemaStub {
 public:
  explicit DefaultSchemaStub(
      std::unique_ptr<google::pubsub::v1::SchemaService::StubInterface> impl)
      : grpc_stub_(std::move(impl)) {}
  ~DefaultSchemaStub() override = default;

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
  std::unique_ptr<google::pubsub::v1::SchemaService::StubInterface> grpc_stub_;
};

/**
 * Creates a SchemaStub with a pre-configured channel.
 */
std::shared_ptr<SchemaStub> CreateDefaultSchemaStub(
    std::shared_ptr<grpc::Channel> channel);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SCHEMA_STUB_H
