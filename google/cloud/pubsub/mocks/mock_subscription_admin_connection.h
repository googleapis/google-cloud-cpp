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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_SUBSCRIPTION_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_SUBSCRIPTION_ADMIN_CONNECTION_H

#include "google/cloud/pubsub/subscription_admin_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_mocks {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A googlemock-based mock for
 * [pubsub::SubscriptionAdminConnection][mocked-link]
 *
 * [mocked-link]: @ref google::cloud::pubsub::v1::SubscriptionAdminConnection
 */
class MockSubscriptionAdminConnection
    : public pubsub::SubscriptionAdminConnection {
 public:
  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, CreateSubscription,
              (CreateSubscriptionParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, GetSubscription,
              (GetSubscriptionParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Subscription>, UpdateSubscription,
              (UpdateSubscriptionParams), (override));

  MOCK_METHOD(pubsub::ListSubscriptionsRange, ListSubscriptions,
              (ListSubscriptionsParams), (override));

  MOCK_METHOD(Status, DeleteSubscription, (DeleteSubscriptionParams),
              (override));

  MOCK_METHOD(Status, ModifyPushConfig, (ModifyPushConfigParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, CreateSnapshot,
              (CreateSnapshotParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, GetSnapshot,
              (GetSnapshotParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::Snapshot>, UpdateSnapshot,
              (UpdateSnapshotParams), (override));

  MOCK_METHOD(pubsub::ListSnapshotsRange, ListSnapshots, (ListSnapshotsParams),
              (override));

  MOCK_METHOD(Status, DeleteSnapshot, (DeleteSnapshotParams), (override));

  MOCK_METHOD(StatusOr<google::pubsub::v1::SeekResponse>, Seek, (SeekParams),
              (override));
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MOCKS_MOCK_SUBSCRIPTION_ADMIN_CONNECTION_H
