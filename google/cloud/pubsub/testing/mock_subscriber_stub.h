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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIBER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIBER_STUB_H

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock pubsub_internal::SubscriberStub
 */
class MockSubscriberStub : public pubsub_internal::SubscriberStub {
 public:
  ~MockSubscriberStub() override = default;

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, CreateSubscription,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::Subscription const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, GetSubscription,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::GetSubscriptionRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, UpdateSubscription,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::UpdateSubscriptionRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListSubscriptionsResponse>,
              ListSubscriptions,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::ListSubscriptionsRequest const&),
              (override));

  MOCK_METHOD(Status, DeleteSubscription,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::DeleteSubscriptionRequest const& request),
              (override));

  MOCK_METHOD(Status, ModifyPushConfig,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::ModifyPushConfigRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::PullResponse>, Pull,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::PullRequest const&),
              (override));

  using StreamingPullStream = google::cloud::AsyncStreamingReadWriteRpc<
      google::pubsub::v1::StreamingPullRequest,
      google::pubsub::v1::StreamingPullResponse>;

  MOCK_METHOD(std::unique_ptr<StreamingPullStream>, AsyncStreamingPull,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions),
              (override));

  MOCK_METHOD(future<Status>, AsyncAcknowledge,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::pubsub::v1::AcknowledgeRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncModifyAckDeadline,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::pubsub::v1::ModifyAckDeadlineRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, CreateSnapshot,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::CreateSnapshotRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, GetSnapshot,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::GetSnapshotRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListSnapshotsResponse>,
              ListSnapshots,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::ListSnapshotsRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, UpdateSnapshot,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::UpdateSnapshotRequest const&),
              (override));

  MOCK_METHOD(Status, DeleteSnapshot,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::DeleteSnapshotRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::SeekResponse>, Seek,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::SeekRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&, Options const&,
               google::iam::v1::SetIamPolicyRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&, Options const&,
               google::iam::v1::GetIamPolicyRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&, Options const&,
               google::iam::v1::TestIamPermissionsRequest const&),
              (override));
};

class MockAsyncPullStream : public MockSubscriberStub::StreamingPullStream {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<google::pubsub::v1::StreamingPullResponse>>,
              Read, (), (override));
  MOCK_METHOD(future<bool>, Write,
              (google::pubsub::v1::StreamingPullRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIBER_STUB_H
