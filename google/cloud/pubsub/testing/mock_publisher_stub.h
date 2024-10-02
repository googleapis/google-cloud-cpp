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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_PUBLISHER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_PUBLISHER_STUB_H

#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock pubsub_internal::PublisherStub
 */
class MockPublisherStub : public pubsub_internal::PublisherStub {
 public:
  ~MockPublisherStub() override = default;

  MOCK_METHOD(StatusOr<google::pubsub::v1::Topic>, CreateTopic,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::Topic const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Topic>, GetTopic,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::GetTopicRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Topic>, UpdateTopic,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::UpdateTopicRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListTopicsResponse>, ListTopics,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::ListTopicsRequest const&),
              (override));

  MOCK_METHOD(Status, DeleteTopic,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::DeleteTopicRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::DetachSubscriptionResponse>,
              DetachSubscription,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::DetachSubscriptionRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListTopicSubscriptionsResponse>,
              ListTopicSubscriptions,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::ListTopicSubscriptionsRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::ListTopicSnapshotsResponse>,
              ListTopicSnapshots,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::ListTopicSnapshotsRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::pubsub::v1::PublishResponse>>,
              AsyncPublish,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::pubsub::v1::PublishRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::PublishResponse>, Publish,
              (grpc::ClientContext&, Options const&,
               google::pubsub::v1::PublishRequest const&),
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_MOCK_PUBLISHER_STUB_H
