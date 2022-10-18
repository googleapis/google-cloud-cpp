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

#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/pubsub/v1/schema.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::pubsub::v1::Schema> DefaultSchemaStub::CreateSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::CreateSchemaRequest const& request) {
  google::pubsub::v1::Schema response;
  auto status = grpc_stub_->CreateSchema(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::Schema> DefaultSchemaStub::GetSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSchemaRequest const& request) {
  google::pubsub::v1::Schema response;
  auto status = grpc_stub_->GetSchema(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::ListSchemasResponse>
DefaultSchemaStub::ListSchemas(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSchemasRequest const& request) {
  google::pubsub::v1::ListSchemasResponse response;
  auto status = grpc_stub_->ListSchemas(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

Status DefaultSchemaStub::DeleteSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSchemaRequest const& request) {
  google::protobuf::Empty response;
  auto status = grpc_stub_->DeleteSchema(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return Status{};
}

StatusOr<google::pubsub::v1::ValidateSchemaResponse>
DefaultSchemaStub::ValidateSchema(
    grpc::ClientContext& context,
    google::pubsub::v1::ValidateSchemaRequest const& request) {
  google::pubsub::v1::ValidateSchemaResponse response;
  auto status = grpc_stub_->ValidateSchema(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::ValidateMessageResponse>
DefaultSchemaStub::ValidateMessage(
    grpc::ClientContext& context,
    google::pubsub::v1::ValidateMessageRequest const& request) {
  google::pubsub::v1::ValidateMessageResponse response;
  auto status = grpc_stub_->ValidateMessage(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
