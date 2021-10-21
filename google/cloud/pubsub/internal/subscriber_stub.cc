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
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/pubsub/v1/pubsub.grpc.pb.h>

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

  StatusOr<google::pubsub::v1::Subscription> UpdateSubscription(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateSubscriptionRequest const& request) override {
    google::pubsub::v1::Subscription response;
    auto status = grpc_stub_->UpdateSubscription(&context, request, &response);
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

  Status ModifyPushConfig(
      grpc::ClientContext& context,
      google::pubsub::v1::ModifyPushConfigRequest const& request) override {
    google::protobuf::Empty response;
    auto status = grpc_stub_->ModifyPushConfig(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return {};
  }

  std::unique_ptr<AsyncPullStream> AsyncStreamingPull(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::pubsub::v1::StreamingPullRequest const&) override {
    return google::cloud::internal::MakeStreamingReadWriteRpc<
        google::pubsub::v1::StreamingPullRequest,
        google::pubsub::v1::StreamingPullResponse>(
        cq, std::move(context),
        [this](grpc::ClientContext* context, grpc::CompletionQueue* cq) {
          return grpc_stub_->PrepareAsyncStreamingPull(context, cq);
        });
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
          auto result = f.get();
          if (!result) return std::move(result).status();
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
          auto result = f.get();
          if (!result) return std::move(result).status();
          return Status{};
        });
  }

  StatusOr<google::pubsub::v1::Snapshot> CreateSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::CreateSnapshotRequest const& request) override {
    google::pubsub::v1::Snapshot response;
    auto status = grpc_stub_->CreateSnapshot(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::Snapshot> GetSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::GetSnapshotRequest const& request) override {
    google::pubsub::v1::Snapshot response;
    auto status = grpc_stub_->GetSnapshot(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::ListSnapshotsResponse> ListSnapshots(
      grpc::ClientContext& context,
      google::pubsub::v1::ListSnapshotsRequest const& request) override {
    google::pubsub::v1::ListSnapshotsResponse response;
    auto status = grpc_stub_->ListSnapshots(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::pubsub::v1::Snapshot> UpdateSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::UpdateSnapshotRequest const& request) override {
    google::pubsub::v1::Snapshot response;
    auto status = grpc_stub_->UpdateSnapshot(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteSnapshot(
      grpc::ClientContext& context,
      google::pubsub::v1::DeleteSnapshotRequest const& request) override {
    google::protobuf::Empty response;
    auto status = grpc_stub_->DeleteSnapshot(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return {};
  }

  StatusOr<google::pubsub::v1::SeekResponse> Seek(
      grpc::ClientContext& context,
      google::pubsub::v1::SeekRequest const& request) override {
    google::pubsub::v1::SeekResponse response;
    auto status = grpc_stub_->Seek(&context, request, &response);
    if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
    return response;
  }

 private:
  std::unique_ptr<google::pubsub::v1::Subscriber::StubInterface> grpc_stub_;
};

std::shared_ptr<SubscriberStub> CreateDefaultSubscriberStub(
    pubsub::ConnectionOptions options, int channel_id) {
  return std::make_shared<DefaultSubscriberStub>(
      google::pubsub::v1::Subscriber::NewStub(
          CreateChannel(std::move(options), channel_id)));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
