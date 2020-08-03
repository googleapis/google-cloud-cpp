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

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/internal/emulator_overrides.h"
#include "google/cloud/grpc_error_delegate.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class DefaultSubscriberStub : public SubscriberStub {
 public:
  explicit DefaultSubscriberStub(
      std::unique_ptr<google::pubsub::v1::Subscriber::StubInterface> grpc_stub)
      : grpc_stub_(std::move(grpc_stub)) {}

  ~DefaultSubscriberStub() override = default;

  StatusOr<google::pubsub::v1::Subscription> CreateSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::Subscription const& request) override {
    google::pubsub::v1::Subscription response;
    auto status = grpc_stub_->CreateSubscription(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::Subscription> GetSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSubscriptionRequest const& request) override {
    google::pubsub::v1::Subscription response;
    auto status = grpc_stub_->GetSubscription(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::ListSubscriptionsResponse> ListSubscriptions(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSubscriptionsRequest const& request) override {
    google::pubsub::v1::ListSubscriptionsResponse response;
    auto status = grpc_stub_->ListSubscriptions(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSubscriptionRequest const& request) override {
    google::protobuf::Empty response;
    auto status = grpc_stub_->DeleteSubscription(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return {};
  }

  future<StatusOr<google::pubsub::v1::PullResponse>> AsyncPull(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::PullRequest const& request) override {
    return cq.MakeUnaryRpc(
        [this](grpc::ClientContext* context,
               google::pubsub::v1::PullRequest const& request,
               grpc::CompletionQueue* cq) {
          return grpc_stub_->AsyncPull(context, request, cq);
        },
        request, std::move(context));
  }

  future<Status> AsyncAcknowledge(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::AcknowledgeRequest const& request) override {
    return cq
        .MakeUnaryRpc(
            [this](grpc::ClientContext* context,
                   google::pubsub::v1::AcknowledgeRequest const& request,
                   grpc::CompletionQueue* cq) {
              return grpc_stub_->AsyncAcknowledge(context, request, cq);
            },
            request, std::move(context))
        .then([](future<StatusOr<google::protobuf::Empty>> f) {
          auto r = f.get();
          if (!r) return std::move(r).status();
          return Status{};
        });
  }

  future<Status> AsyncModifyAckDeadline(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::ModifyAckDeadlineRequest const& request) override {
    return cq
        .MakeUnaryRpc(
            [this](grpc::ClientContext* context,
                   google::pubsub::v1::ModifyAckDeadlineRequest const& request,
                   grpc::CompletionQueue* cq) {
              return grpc_stub_->AsyncModifyAckDeadline(context, request, cq);
            },
            request, std::move(context))
        .then([](future<StatusOr<google::protobuf::Empty>> f) {
          auto r = f.get();
          if (!r) return std::move(r).status();
          return Status{};
        });
  }

 private:
  std::unique_ptr<google::pubsub::v1::Subscriber::StubInterface> grpc_stub_;
};

std::shared_ptr<SubscriberStub> CreateDefaultSubscriberStub(
    pubsub::ConnectionOptions options, int channel_id) {
  options = EmulatorOverrides(std::move(options));
  auto channel_arguments = options.CreateChannelArguments();
  // Newer versions of gRPC include a macro (`GRPC_ARG_CHANNEL_ID`) but use
  // its value here to allow compiling against older versions.
  channel_arguments.SetInt("grpc.channel_id", channel_id);
  auto channel = grpc::CreateCustomChannel(
      options.endpoint(), options.credentials(), channel_arguments);

  auto grpc_stub =
      google::pubsub::v1::Subscriber::NewStub(grpc::CreateCustomChannel(
          options.endpoint(), options.credentials(), channel_arguments));

  return std::make_shared<DefaultSubscriberStub>(std::move(grpc_stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
