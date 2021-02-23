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

#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/pubsub/v1/schema.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

class DefaultSchemaStub : public SchemaStub {
 public:
  explicit DefaultSchemaStub(
      std::unique_ptr<google::pubsub::v1::SchemaService::StubInterface> impl)
      : impl_(std::move(impl)) {}

  ~DefaultSchemaStub() override = default;

  StatusOr<google::pubsub::v1::Schema> CreateSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::CreateSchemaRequest const& request) override {
    google::pubsub::v1::Schema response;
    auto status = impl_->CreateSchema(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::Schema> GetSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSchemaRequest const& request) override {
    google::pubsub::v1::Schema response;
    auto status = impl_->GetSchema(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::ListSchemasResponse> ListSchemas(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSchemasRequest const& request) override {
    google::pubsub::v1::ListSchemasResponse response;
    auto status = impl_->ListSchemas(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSchemaRequest const& request) override {
    google::protobuf::Empty response;
    auto status = impl_->DeleteSchema(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::pubsub::v1::ValidateSchemaResponse> ValidateSchema(
      grpc::ClientContext& context,
      google::pubsub::v1::ValidateSchemaRequest const& request) override {
    google::pubsub::v1::ValidateSchemaResponse response;
    auto status = impl_->ValidateSchema(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::ValidateMessageResponse> ValidateMessage(
      grpc::ClientContext& context,
      google::pubsub::v1::ValidateMessageRequest const& request) override {
    google::pubsub::v1::ValidateMessageResponse response;
    auto status = impl_->ValidateMessage(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

 private:
  std::unique_ptr<google::pubsub::v1::SchemaService::StubInterface> impl_;
};
}  // namespace

std::shared_ptr<SchemaStub> CreateDefaultSchemaStub(
    pubsub::ConnectionOptions options, int channel_id) {
  return std::make_shared<DefaultSchemaStub>(
      google::pubsub::v1::SchemaService::NewStub(
          CreateChannel(std::move(options), channel_id)));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
