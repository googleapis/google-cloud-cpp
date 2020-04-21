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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_CREATE_SUBSCRIPTION_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_CREATE_SUBSCRIPTION_BUILDER_H

#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/topic.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Helper class to create google::pubsub::v1::PushConfig protos.
 */
class PushConfigBuilder {
 public:
  explicit PushConfigBuilder(std::string push_endpoint) {
    proto_.set_push_endpoint(std::move(push_endpoint));
  }

  PushConfigBuilder& add_attribute(std::string const& key,
                                   std::string const& value) {
    proto_.mutable_attributes()->insert(
        google::protobuf::Map<std::string, std::string>::value_type(key,
                                                                    value));
    return *this;
  }
  PushConfigBuilder& set_attributes(
      std::vector<std::pair<std::string, std::string>> attr) {
    google::protobuf::Map<std::string, std::string> attributes;
    for (auto& kv : attr) {
      attributes[kv.first] = std::move(kv.second);
    }
    proto_.mutable_attributes()->swap(attributes);
    return *this;
  }

  static google::pubsub::v1::PushConfig::OidcToken MakeOidcToken(
      std::string service_account_email) {
    google::pubsub::v1::PushConfig::OidcToken proto;
    proto.set_service_account_email(std::move(service_account_email));
    return proto;
  }

  static google::pubsub::v1::PushConfig::OidcToken MakeOidcToken(
      std::string service_account_email, std::string audience) {
    google::pubsub::v1::PushConfig::OidcToken proto;
    proto.set_service_account_email(std::move(service_account_email));
    proto.set_audience(std::move(audience));
    return proto;
  }

  PushConfigBuilder& set_authentication(
      google::pubsub::v1::PushConfig::OidcToken token) {
    *proto_.mutable_oidc_token() = std::move(token);
    return *this;
  }

  google::pubsub::v1::PushConfig as_proto() const& { return proto_; }
  google::pubsub::v1::PushConfig&& as_proto() && { return std::move(proto_); }

 private:
  google::pubsub::v1::PushConfig proto_;
};

/**
 * Create a Cloud Pub/Sub subscription configuration.
 */
class CreateSubscriptionBuilder {
 public:
  explicit CreateSubscriptionBuilder(Subscription const& subscription,
                                     Topic const& topic) {
    proto_.set_name(subscription.FullName());
    proto_.set_topic(topic.FullName());
  }

  CreateSubscriptionBuilder& set_push_config(google::pubsub::v1::PushConfig v) {
    *proto_.mutable_push_config() = std::move(v);
    return *this;
  }

  CreateSubscriptionBuilder& set_ack_deadline(std::chrono::seconds v) {
    proto_.set_ack_deadline_seconds(static_cast<std::int32_t>(v.count()));
    return *this;
  }

  CreateSubscriptionBuilder& set_retain_acked_messages(bool v) {
    proto_.set_retain_acked_messages(v);
    return *this;
  }

  template <typename Rep, typename Period>
  CreateSubscriptionBuilder& set_message_retention_duration(
      std::chrono::duration<Rep, Period> d) {
    *proto_.mutable_message_retention_duration() =
        ToDurationProto(std::move(d));
    return *this;
  }

  CreateSubscriptionBuilder& add_label(std::string const& key,
                                       std::string const& value) {
    using value_type = protobuf::Map<std::string, std::string>::value_type;
    proto_.mutable_labels()->insert(value_type(key, value));
    return *this;
  }
  CreateSubscriptionBuilder& set_labels(
      std::vector<std::pair<std::string, std::string>> new_labels) {
    google::protobuf::Map<std::string, std::string> labels;
    for (auto& kv : new_labels) {
      labels[kv.first] = std::move(kv.second);
    }
    proto_.mutable_labels()->swap(labels);
    return *this;
  }
  CreateSubscriptionBuilder& clear_labels() {
    proto_.clear_labels();
    return *this;
  }

  CreateSubscriptionBuilder& enable_message_ordering(bool v) {
    proto_.set_enable_message_ordering(v);
    return *this;
  }

  CreateSubscriptionBuilder& set_expiration_policy(
      google::pubsub::v1::ExpirationPolicy v) {
    *proto_.mutable_expiration_policy() = std::move(v);
    return *this;
  }

  CreateSubscriptionBuilder& set_dead_letter_policy(
      google::pubsub::v1::DeadLetterPolicy v) {
    *proto_.mutable_dead_letter_policy() = std::move(v);
    return *this;
  }

  template <typename Rep, typename Period>
  static google::pubsub::v1::ExpirationPolicy MakeExpirationPolicy(
      std::chrono::duration<Rep, Period> d) {
    google::pubsub::v1::ExpirationPolicy result;
    *result.mutable_ttl() = ToDurationProto(std::move(d));
    return result;
  }

  static google::pubsub::v1::DeadLetterPolicy MakeDeadLetterPolicy(
      Topic const& dead_letter_topic, std::int32_t max_delivery_attemps = 0) {
    google::pubsub::v1::DeadLetterPolicy result;
    result.set_dead_letter_topic(dead_letter_topic.FullName());
    result.set_max_delivery_attempts(max_delivery_attemps);
    return result;
  }

  google::pubsub::v1::Subscription as_proto() const& { return proto_; }
  google::pubsub::v1::Subscription&& as_proto() && { return std::move(proto_); }

 private:
  template <typename Rep, typename Period>
  static google::protobuf::Duration ToDurationProto(
      std::chrono::duration<Rep, Period> d) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(d - seconds);
    google::protobuf::Duration result;
    result.set_seconds(seconds.count());
    result.set_nanos(static_cast<std::int32_t>(nanos.count()));
    return result;
  }

  google::pubsub::v1::Subscription proto_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_CREATE_SUBSCRIPTION_BUILDER_H
