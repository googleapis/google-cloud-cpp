// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include <google/pubsub/v1/pubsub.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::pubsub::v1::Topic> DefaultPublisherStub::CreateTopic(
    grpc::ClientContext& context, google::pubsub::v1::Topic const& request) {
  google::pubsub::v1::Topic response;
  auto status = grpc_stub_->CreateTopic(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);

  return response;
}

StatusOr<google::pubsub::v1::Topic> DefaultPublisherStub::GetTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::GetTopicRequest const& request) {
  google::pubsub::v1::Topic response;
  auto status = grpc_stub_->GetTopic(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::Topic> DefaultPublisherStub::UpdateTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateTopicRequest const& request) {
  google::pubsub::v1::Topic response;
  auto status = grpc_stub_->UpdateTopic(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::ListTopicsResponse>
DefaultPublisherStub::ListTopics(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicsRequest const& request) {
  google::pubsub::v1::ListTopicsResponse response;
  auto status = grpc_stub_->ListTopics(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

Status DefaultPublisherStub::DeleteTopic(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteTopicRequest const& request) {
  google::protobuf::Empty response;
  auto status = grpc_stub_->DeleteTopic(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return {};
}

StatusOr<google::pubsub::v1::DetachSubscriptionResponse>
DefaultPublisherStub::DetachSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DetachSubscriptionRequest const& request) {
  google::pubsub::v1::DetachSubscriptionResponse response;
  auto status = grpc_stub_->DetachSubscription(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::ListTopicSubscriptionsResponse>
DefaultPublisherStub::ListTopicSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSubscriptionsRequest const& request) {
  google::pubsub::v1::ListTopicSubscriptionsResponse response;
  auto status =
      grpc_stub_->ListTopicSubscriptions(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse>
DefaultPublisherStub::ListTopicSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListTopicSnapshotsRequest const& request) {
  google::pubsub::v1::ListTopicSnapshotsResponse response;
  auto status = grpc_stub_->ListTopicSnapshots(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

future<StatusOr<google::pubsub::v1::PublishResponse>>
DefaultPublisherStub::AsyncPublish(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::PublishRequest const& request) {
  return cq.MakeUnaryRpc(
      [this](grpc::ClientContext* context,
             google::pubsub::v1::PublishRequest const& request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncPublish(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::pubsub::v1::PublishResponse> DefaultPublisherStub::Publish(
    grpc::ClientContext& context,
    google::pubsub::v1::PublishRequest const& request) {
  google::pubsub::v1::PublishResponse response;
  auto status = grpc_stub_->Publish(&context, request, &response);
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
