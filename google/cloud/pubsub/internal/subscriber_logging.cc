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

#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

using ::google::cloud::internal::LogWrapper;

StatusOr<google::pubsub::v1::Subscription>
SubscriberLogging::CreateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::Subscription const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::Subscription const& request) {
        return child_->CreateSubscription(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Subscription> SubscriberLogging::GetSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSubscriptionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::GetSubscriptionRequest const& request) {
        return child_->GetSubscription(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Subscription>
SubscriberLogging::UpdateSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSubscriptionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::UpdateSubscriptionRequest const& request) {
        return child_->UpdateSubscription(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ListSubscriptionsResponse>
SubscriberLogging::ListSubscriptions(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSubscriptionsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ListSubscriptionsRequest const& request) {
        return child_->ListSubscriptions(context, request);
      },
      context, request, __func__, tracing_options_);
}

Status SubscriberLogging::DeleteSubscription(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSubscriptionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::DeleteSubscriptionRequest const& request) {
        return child_->DeleteSubscription(context, request);
      },
      context, request, __func__, tracing_options_);
}

Status SubscriberLogging::ModifyPushConfig(
    grpc::ClientContext& context,
    google::pubsub::v1::ModifyPushConfigRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ModifyPushConfigRequest const& request) {
        return child_->ModifyPushConfig(context, request);
      },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::pubsub::v1::PullResponse>> SubscriberLogging::AsyncPull(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::PullRequest const& request) {
  return LogWrapper(
      [this](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::PullRequest const& request) {
        return child_->AsyncPull(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

future<Status> SubscriberLogging::AsyncAcknowledge(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::AcknowledgeRequest const& request) {
  return LogWrapper(
      [this](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::AcknowledgeRequest const& request) {
        return child_->AsyncAcknowledge(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

future<Status> SubscriberLogging::AsyncModifyAckDeadline(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
  return LogWrapper(
      [this](google::cloud::CompletionQueue& cq,
             std::unique_ptr<grpc::ClientContext> context,
             google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
        return child_->AsyncModifyAckDeadline(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberLogging::CreateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::CreateSnapshotRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::CreateSnapshotRequest const& request) {
        return child_->CreateSnapshot(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::ListSnapshotsResponse>
SubscriberLogging::ListSnapshots(
    grpc::ClientContext& context,
    google::pubsub::v1::ListSnapshotsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::ListSnapshotsRequest const& request) {
        return child_->ListSnapshots(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberLogging::GetSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::GetSnapshotRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::GetSnapshotRequest const& request) {
        return child_->GetSnapshot(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::Snapshot> SubscriberLogging::UpdateSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::UpdateSnapshotRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::UpdateSnapshotRequest const& request) {
        return child_->UpdateSnapshot(context, request);
      },
      context, request, __func__, tracing_options_);
}

Status SubscriberLogging::DeleteSnapshot(
    grpc::ClientContext& context,
    google::pubsub::v1::DeleteSnapshotRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::DeleteSnapshotRequest const& request) {
        return child_->DeleteSnapshot(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::pubsub::v1::SeekResponse> SubscriberLogging::Seek(
    grpc::ClientContext& context,
    google::pubsub::v1::SeekRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::pubsub::v1::SeekRequest const& request) {
        return child_->Seek(context, request);
      },
      context, request, __func__, tracing_options_);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
