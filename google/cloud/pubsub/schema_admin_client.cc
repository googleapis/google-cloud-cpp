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

#include "google/cloud/pubsub/schema_admin_client.h"
#include "google/cloud/pubsub/internal/defaults.h"

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

SchemaAdminClient::SchemaAdminClient(
    std::shared_ptr<SchemaAdminConnection> connection, Options opts)
    : connection_(std::move(connection)),
      options_(
          internal::MergeOptions(std::move(opts), connection_->options())) {}

StatusOr<google::pubsub::v1::Schema> SchemaAdminClient::CreateAvroSchema(
    Schema const& schema, std::string schema_definition, Options opts) {
  google::pubsub::v1::CreateSchemaRequest request;
  request.set_parent("projects/" + schema.project_id());
  request.set_schema_id(schema.schema_id());
  request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
  request.mutable_schema()->set_definition(std::move(schema_definition));
  return CreateSchema(request, std::move(opts));
}

StatusOr<google::pubsub::v1::Schema> SchemaAdminClient::CreateProtobufSchema(
    Schema const& schema, std::string schema_definition, Options opts) {
  google::pubsub::v1::CreateSchemaRequest request;
  request.set_parent("projects/" + schema.project_id());
  request.set_schema_id(schema.schema_id());
  request.mutable_schema()->set_type(
      google::pubsub::v1::Schema::PROTOCOL_BUFFER);
  request.mutable_schema()->set_definition(std::move(schema_definition));
  return CreateSchema(request, std::move(opts));
}

StatusOr<google::pubsub::v1::Schema> SchemaAdminClient::CreateSchema(
    google::pubsub::v1::CreateSchemaRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CreateSchema(request);
}

StatusOr<google::pubsub::v1::Schema> SchemaAdminClient::GetSchema(
    Schema const& schema, google::pubsub::v1::SchemaView view, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::pubsub::v1::GetSchemaRequest request;
  request.set_name(schema.FullName());
  request.set_view(view);
  return connection_->GetSchema(request);
}

ListSchemasRange SchemaAdminClient::ListSchemas(
    std::string const& project_id, google::pubsub::v1::SchemaView view,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::pubsub::v1::ListSchemasRequest request;
  request.set_parent("projects/" + project_id);
  request.set_view(view);
  return connection_->ListSchemas(request);
}

Status SchemaAdminClient::DeleteSchema(Schema const& schema, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::pubsub::v1::DeleteSchemaRequest request;
  request.set_name(schema.FullName());
  return connection_->DeleteSchema(request);
}

StatusOr<google::pubsub::v1::ValidateSchemaResponse>
SchemaAdminClient::ValidateAvroSchema(std::string const& project_id,
                                      std::string schema_definition,
                                      Options opts) {
  google::pubsub::v1::Schema schema;
  schema.set_definition(std::move(schema_definition));
  schema.set_type(google::pubsub::v1::Schema::AVRO);
  return ValidateSchema(project_id, std::move(schema), std::move(opts));
}

StatusOr<google::pubsub::v1::ValidateSchemaResponse>
SchemaAdminClient::ValidateProtobufSchema(std::string const& project_id,
                                          std::string schema_definition,
                                          Options opts) {
  google::pubsub::v1::Schema schema;
  schema.set_definition(std::move(schema_definition));
  schema.set_type(google::pubsub::v1::Schema::PROTOCOL_BUFFER);
  return ValidateSchema(project_id, std::move(schema), std::move(opts));
}

StatusOr<google::pubsub::v1::ValidateSchemaResponse>
SchemaAdminClient::ValidateSchema(std::string const& project_id,
                                  google::pubsub::v1::Schema schema,
                                  Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::pubsub::v1::ValidateSchemaRequest request;
  request.set_parent("projects/" + project_id);
  *request.mutable_schema() = std::move(schema);
  return connection_->ValidateSchema(request);
}

StatusOr<google::pubsub::v1::ValidateMessageResponse>
SchemaAdminClient::ValidateMessageWithNamedSchema(
    google::pubsub::v1::Encoding encoding, std::string message,
    Schema const& named_schema, Options opts) {
  google::pubsub::v1::ValidateMessageRequest request;
  request.set_parent("projects/" + named_schema.project_id());
  request.set_message(std::move(message));
  request.set_encoding(encoding);
  request.set_name(named_schema.FullName());
  return ValidateMessage(request, std::move(opts));
}

StatusOr<google::pubsub::v1::ValidateMessageResponse>
SchemaAdminClient::ValidateMessageWithAvro(
    google::pubsub::v1::Encoding encoding, std::string message,
    std::string project_id, std::string schema_definition, Options opts) {
  google::pubsub::v1::ValidateMessageRequest request;
  request.set_parent("projects/" + std::move(project_id));
  request.set_message(std::move(message));
  request.set_encoding(encoding);
  request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
  request.mutable_schema()->set_definition(std::move(schema_definition));
  return ValidateMessage(request, std::move(opts));
}

StatusOr<google::pubsub::v1::ValidateMessageResponse>
SchemaAdminClient::ValidateMessageWithProtobuf(
    google::pubsub::v1::Encoding encoding, std::string message,
    std::string project_id, std::string schema_definition, Options opts) {
  google::pubsub::v1::ValidateMessageRequest request;
  request.set_parent("projects/" + std::move(project_id));
  request.set_message(std::move(message));
  request.set_encoding(encoding);
  request.mutable_schema()->set_type(
      google::pubsub::v1::Schema::PROTOCOL_BUFFER);
  request.mutable_schema()->set_definition(std::move(schema_definition));
  return ValidateMessage(request, std::move(opts));
}

StatusOr<google::pubsub::v1::ValidateMessageResponse>
SchemaAdminClient::ValidateMessage(
    google::pubsub::v1::ValidateMessageRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ValidateMessage(request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
