// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/internal/emulator_overrides.h"
#include "google/cloud/grpc_error_delegate.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class DefaultPublisherStub : public PublisherStub {
 public:
  explicit DefaultPublisherStub(
      std::unique_ptr<google::pubsub::v1::Publisher::StubInterface> grpc_stub)
      : grpc_stub_(std::move(grpc_stub)) {}

  ~DefaultPublisherStub() override = default;

  StatusOr<google::pubsub::v1::Topic> CreateTopic(
      grpc::ClientContext& context,
      google::pubsub::v1::Topic const& request) override {
    google::pubsub::v1::Topic response;
    auto status = grpc_stub_->CreateTopic(&context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::pubsub::v1::ListTopicsResponse> ListTopics(
      grpc::ClientContext& context,
      google::pubsub::v1::ListTopicsRequest const& request) override {
    google::pubsub::v1::ListTopicsResponse response;
    auto status = grpc_stub_->ListTopics(&context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  Status DeleteTopic(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteTopicRequest const& request) override {
    google::protobuf::Empty response;
    auto status = grpc_stub_->DeleteTopic(&context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return {};
  }

  future<StatusOr<google::pubsub::v1::PublishResponse>> AsyncPublish(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::PublishRequest const& request) override {
    return cq.MakeUnaryRpc(
        [this](grpc::ClientContext* context,
               google::pubsub::v1::PublishRequest const& request,
               grpc::CompletionQueue* cq) {
          return grpc_stub_->AsyncPublish(context, request, cq);
        },
        request, std::move(context));
  }

 private:
  std::unique_ptr<google::pubsub::v1::Publisher::StubInterface> grpc_stub_;
};

std::shared_ptr<PublisherStub> CreateDefaultPublisherStub(
    pubsub::ConnectionOptions options, int channel_id) {
  options = EmulatorOverrides(std::move(options));
  auto channel_arguments = options.CreateChannelArguments();
  // Newer versions of gRPC include a macro (`GRPC_ARG_CHANNEL_ID`) but use
  // its value here to allow compiling against older versions.
  channel_arguments.SetInt("grpc.channel_id", channel_id);
  auto channel = grpc::CreateCustomChannel(
      options.endpoint(), options.credentials(), channel_arguments);

  auto grpc_stub =
      google::pubsub::v1::Publisher::NewStub(grpc::CreateCustomChannel(
          options.endpoint(), options.credentials(), channel_arguments));

  return std::make_shared<DefaultPublisherStub>(std::move(grpc_stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
