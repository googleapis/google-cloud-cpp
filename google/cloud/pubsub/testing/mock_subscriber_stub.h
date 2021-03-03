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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIBER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIBER_STUB_H

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A class to mock pubsub_internal::SubscriberStub
 */
class MockSubscriberStub : public pubsub_internal::SubscriberStub {
 public:
  ~MockSubscriberStub() override = default;

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, CreateSubscription,
              (grpc::ClientContext&, google::pubsub::v1::Subscription const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, GetSubscription,
              (grpc::ClientContext&,
               google::pubsub::v1::GetSubscriptionRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, UpdateSubscription,
              (grpc::ClientContext&,
               google::pubsub::v1::UpdateSubscriptionRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListSubscriptionsResponse>,
              ListSubscriptions,
              (grpc::ClientContext&,
               google::pubsub::v1::ListSubscriptionsRequest const&),
              (override));

  MOCK_METHOD(Status, DeleteSubscription,
              (grpc::ClientContext&,
               google::pubsub::v1::DeleteSubscriptionRequest const& request),
              (override));

  MOCK_METHOD(Status, ModifyPushConfig,
              (grpc::ClientContext&,
               google::pubsub::v1::ModifyPushConfigRequest const& request),
              (override));

  MOCK_METHOD(std::unique_ptr<pubsub_internal::SubscriberStub::AsyncPullStream>,
              AsyncStreamingPull,
              (google::cloud::CompletionQueue&,
               std::unique_ptr<grpc::ClientContext>,
               google::pubsub::v1::StreamingPullRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, CreateSnapshot,
              (grpc::ClientContext&,
               google::pubsub::v1::CreateSnapshotRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, GetSnapshot,
              (grpc::ClientContext&,
               google::pubsub::v1::GetSnapshotRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListSnapshotsResponse>,
              ListSnapshots,
              (grpc::ClientContext&,
               google::pubsub::v1::ListSnapshotsRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, UpdateSnapshot,
              (grpc::ClientContext&,
               google::pubsub::v1::UpdateSnapshotRequest const&),
              (override));

  MOCK_METHOD(Status, DeleteSnapshot,
              (grpc::ClientContext&,
               google::pubsub::v1::DeleteSnapshotRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::SeekResponse>, Seek,
              (grpc::ClientContext&, google::pubsub::v1::SeekRequest const&),
              (override));
};

class MockAsyncPullStream
    : public pubsub_internal::SubscriberStub::AsyncPullStream {
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

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_SUBSCRIBER_STUB_H
