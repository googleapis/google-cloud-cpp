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

#include "google/cloud/pubsub/internal/schema_logging.h"
#include "google/cloud/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

using ::google::cloud::internal::LogWrapper;

StatusOr<google::pubsub::v1::Schema> SchemaLogging::CreateSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::CreateSchemaRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::CreateSchemaRequest const& request) {
        return child_->CreateSchema(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Schema> SchemaLogging::GetSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSchemaRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::GetSchemaRequest const& request) {
        return child_->GetSchema(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ListSchemasResponse> SchemaLogging::ListSchemas(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSchemasRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ListSchemasRequest const& request) {
        return child_->ListSchemas(context, request);
      },
      context, request, __func__, tracing_options_);
}

Status SchemaLogging::DeleteSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSchemaRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::DeleteSchemaRequest const& request) {
        return child_->DeleteSchema(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ValidateSchemaResponse>
SchemaLogging::ValidateSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::ValidateSchemaRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ValidateSchemaRequest const& request) {
        return child_->ValidateSchema(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ValidateMessageResponse>
SchemaLogging::ValidateMessage(
    grpc::ClientContext& context,
    google::pubsub::v1::ValidateMessageRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ValidateMessageRequest const& request) {
        return child_->ValidateMessage(context, request);
      },
      context, request, __func__, tracing_options_);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
