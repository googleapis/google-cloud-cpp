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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_TOPIC_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_TOPIC_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/topic_admin_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A googlemock-based mock for [pubsub::TopicAdminConnection][mocked-link]
 *
 * [mocked-link]: @ref google::cloud::pubsub::TopicAdminConnection
 */
class MockTopicAdminConnection : public pubsub::TopicAdminConnection {
 public:
  MOCK_METHOD(StatusOr<google::pubsub::v1::Topic>, CreateTopic,
              (CreateTopicParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Topic>, GetTopic, (GetTopicParams),
              (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Topic>, UpdateTopic,
              (UpdateTopicParams), (override));

  MOCK_METHOD(pubsub::ListTopicsRange, ListTopics, (ListTopicsParams),
              (override));

  MOCK_METHOD(Status, DeleteTopic, (DeleteTopicParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::DetachSubscriptionResponse>,
              DetachSubscription, (DetachSubscriptionParams), (override));

  MOCK_METHOD(pubsub::ListTopicSubscriptionsRange, ListTopicSubscriptions,
              (ListTopicSubscriptionsParams), (override));

  MOCK_METHOD(pubsub::ListTopicSnapshotsRange, ListTopicSnapshots,
              (ListTopicSnapshotsParams), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_TOPIC_ADMIN_CONNECTION_H
