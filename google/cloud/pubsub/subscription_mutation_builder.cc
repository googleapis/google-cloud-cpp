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

#include "google/cloud/pubsub/subscription_mutation_builder.h"

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

google::pubsub::v1::ModifyPushConfigRequest
PushConfigBuilder::BuildModifyPushConfig(Subscription const& subscription) && {
  google::pubsub::v1::ModifyPushConfigRequest request;
  request.set_subscription(subscription.FullName());
  if (!paths_.empty()) {
    *request.mutable_push_config() = std::move(proto_);
  }
  return request;
}

google::pubsub::v1::UpdateSubscriptionRequest
SubscriptionMutationBuilder::BuildUpdateSubscription(
    Subscription const& subscription) && {
  google::pubsub::v1::UpdateSubscriptionRequest request;
  *request.mutable_subscription() = std::move(proto_);
  request.mutable_subscription()->set_name(subscription.FullName());
  for (auto const& p : paths_) {
    google::protobuf::util::FieldMaskUtil::AddPathToFieldMask<
        google::pubsub::v1::Subscription>(p, request.mutable_update_mask());
  }
  return request;
}

google::pubsub::v1::Subscription
SubscriptionMutationBuilder::BuildCreateSubscription(
    Topic const& topic, Subscription const& subscription) && {
  google::pubsub::v1::Subscription request = std::move(proto_);
  request.set_topic(topic.FullName());
  request.set_name(subscription.FullName());
  return request;
}

SubscriptionMutationBuilder& SubscriptionMutationBuilder::set_push_config(
    PushConfigBuilder v) & {
  if (v.paths_.empty()) {
    proto_.clear_push_config();
    paths_.insert("push_config");
  } else {
    *proto_.mutable_push_config() = std::move(v.proto_);
    for (auto const& s : v.paths_) {
      paths_.insert("push_config." + s);
    }
  }
  return *this;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
