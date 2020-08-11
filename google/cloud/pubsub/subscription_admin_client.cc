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

#include "google/cloud/pubsub/subscription_admin_client.h"

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriptionAdminClient::SubscriptionAdminClient(
    std::shared_ptr<SubscriptionAdminConnection> connection)
    : connection_(std::move(connection)) {}

StatusOr<google::pubsub::v1::SeekResponse> SubscriptionAdminClient::Seek(
    Subscription const& subscription,
    std::chrono::system_clock::time_point timestamp) {
  google::pubsub::v1::SeekRequest request;
  request.set_subscription(subscription.FullName());
  *request.mutable_time() =
      google::cloud::internal::ToProtoTimestamp(timestamp);
  return connection_->Seek({std::move(request)});
}

StatusOr<google::pubsub::v1::SeekResponse> SubscriptionAdminClient::Seek(
    Subscription const& subscription, Snapshot const& snapshot) {
  google::pubsub::v1::SeekRequest request;
  request.set_subscription(subscription.FullName());
  request.set_snapshot(snapshot.FullName());
  return connection_->Seek({std::move(request)});
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
