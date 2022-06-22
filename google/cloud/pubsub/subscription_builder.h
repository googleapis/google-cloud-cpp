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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_BUILDER_H

#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/time_utils.h"
#include <google/protobuf/util/field_mask_util.h>
#include <google/pubsub/v1/pubsub.pb.h>
#include <chrono>
#include <set>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class SubscriptionBuilder;

/**
 * Helper class to create google::pubsub::v1::PushConfig protos.
 *
 * Makes it easier to create the protobuf messages consumed by
 * `SubscriptionAdminClient`.  The main advantages are:
 *
 * - Use a fluent API to set multiple values when constructing complex objects.
 * - Automatically compute the set of paths for update requests.
 */
class PushConfigBuilder {
 public:
  PushConfigBuilder() = default;
  explicit PushConfigBuilder(std::string push_endpoint) {
    set_push_endpoint(std::move(push_endpoint));
  }

  /// Build a protocol buffer message to update an existing push config.
  google::pubsub::v1::ModifyPushConfigRequest BuildModifyPushConfig(
      Subscription const& subscription) &&;

  /// @name Setters for each protocol buffer field.
  ///@{
  PushConfigBuilder& set_push_endpoint(std::string v) & {
    proto_.set_push_endpoint(std::move(v));
    paths_.insert("push_endpoint");
    return *this;
  }
  PushConfigBuilder&& set_push_endpoint(std::string v) && {
    return std::move(set_push_endpoint(std::move(v)));
  }

  PushConfigBuilder& add_attribute(std::string const& key,
                                   std::string const& value) & {
    proto_.mutable_attributes()->insert(
        google::protobuf::Map<std::string, std::string>::value_type(key,
                                                                    value));
    paths_.insert("attributes");
    return *this;
  }
  PushConfigBuilder&& add_attribute(std::string const& key,
                                    std::string const& value) && {
    return std::move(add_attribute(key, value));
  }

  PushConfigBuilder& set_attributes(
      std::vector<std::pair<std::string, std::string>> attr) & {
    google::protobuf::Map<std::string, std::string> attributes;
    for (auto& kv : attr) {
      attributes[kv.first] = std::move(kv.second);
    }
    proto_.mutable_attributes()->swap(attributes);
    paths_.insert("attributes");
    return *this;
  }
  PushConfigBuilder&& set_attributes(
      std::vector<std::pair<std::string, std::string>> attr) && {
    return std::move(set_attributes(std::move(attr)));
  }

  PushConfigBuilder& clear_attributes() & {
    proto_.mutable_attributes()->clear();
    paths_.insert("attributes");
    return *this;
  }
  PushConfigBuilder&& clear_attributes() && {
    return std::move(clear_attributes());
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
      google::pubsub::v1::PushConfig::OidcToken token) & {
    *proto_.mutable_oidc_token() = std::move(token);
    paths_.insert("oidc_token");
    return *this;
  }
  PushConfigBuilder&& set_authentication(
      google::pubsub::v1::PushConfig::OidcToken token) && {
    return std::move(set_authentication(std::move(token)));
  }
  //@}

 private:
  friend class SubscriptionBuilder;
  google::pubsub::v1::PushConfig proto_;
  std::set<std::string> paths_;
};

/**
 * A helper class to build `google::pubsub::v1::BigQueryConfig` protos.
 *
 * Makes it easier to create the protobuf messages consumed by
 * `SubscriptionAdminClient`.  The main advantages are:
 *
 * - Use a fluent API to set multiple values when constructing complex objects.
 * - Automatically compute the set of paths for update requests.
 */
class BigQueryConfigBuilder {
 public:
  BigQueryConfigBuilder() = default;

  /// @name Setters for each protocol buffer field.
  ///@{
  BigQueryConfigBuilder& set_table(std::string full_path) & {
    *proto_.mutable_table() = std::move(full_path);
    paths_.insert("table");
    return *this;
  }
  BigQueryConfigBuilder&& set_table(std::string full_path) && {
    return std::move(set_table(std::move(full_path)));
  }
  BigQueryConfigBuilder& set_table(std::string const& project_id,
                                   std::string const& data_set_id,
                                   std::string const& table_id) & {
    return set_table(project_id + ':' + data_set_id + '.' + table_id);
  }
  BigQueryConfigBuilder&& set_table(std::string const& project_id,
                                    std::string const& data_set_id,
                                    std::string const& table_id) && {
    return std::move(set_table(project_id, data_set_id, table_id));
  }

  BigQueryConfigBuilder& set_use_topic_schema(bool v) & {
    proto_.set_use_topic_schema(v);
    paths_.insert("use_topic_schema");
    return *this;
  }
  BigQueryConfigBuilder&& set_use_topic_schema(bool v) && {
    return std::move(set_use_topic_schema(v));
  }

  BigQueryConfigBuilder& set_write_metadata(bool v) & {
    proto_.set_write_metadata(v);
    paths_.insert("write_metadata");
    return *this;
  }
  BigQueryConfigBuilder&& set_write_metadata(bool v) && {
    return std::move(set_write_metadata(v));
  }

  BigQueryConfigBuilder& set_drop_unknown_fields(bool v) & {
    proto_.set_drop_unknown_fields(v);
    paths_.insert("drop_unknown_fields");
    return *this;
  }
  BigQueryConfigBuilder&& set_drop_unknown_fields(bool v) && {
    return std::move(set_drop_unknown_fields(v));
  }
  ///@}

 private:
  friend class SubscriptionBuilder;
  google::pubsub::v1::BigQueryConfig proto_;
  std::set<std::string> paths_;
};

/**
 * Create a Cloud Pub/Sub subscription configuration.
 *
 * Makes it easier to create the protobuf messages consumed by
 * `SubscriptionAdminClient`.  The main advantages are:
 *
 * - Use a fluent API to set multiple values when constructing complex objects.
 * - Automatically compute the set of paths for update requests.
 */
class SubscriptionBuilder {
 public:
  SubscriptionBuilder() = default;

  /// Build a protocol buffer message to update an existing subscription.
  google::pubsub::v1::UpdateSubscriptionRequest BuildUpdateRequest(
      Subscription const& subscription) &&;

  /// Build a protocol buffer message to create a new subscription.
  google::pubsub::v1::Subscription BuildCreateRequest(
      Topic const& topic, Subscription const& subscription) &&;

  /// @name Setters for each protocol buffer field.
  ///@{
  SubscriptionBuilder& set_push_config(PushConfigBuilder v) &;
  SubscriptionBuilder&& set_push_config(PushConfigBuilder v) && {
    return std::move(set_push_config(std::move(v)));
  }

  SubscriptionBuilder& set_bigquery_config(BigQueryConfigBuilder v) &;
  SubscriptionBuilder&& set_bigquery_config(BigQueryConfigBuilder v) && {
    return std::move(set_bigquery_config(std::move(v)));
  }

  SubscriptionBuilder& set_ack_deadline(std::chrono::seconds v) & {
    proto_.set_ack_deadline_seconds(static_cast<std::int32_t>(v.count()));
    paths_.insert("ack_deadline_seconds");
    return *this;
  }
  SubscriptionBuilder&& set_ack_deadline(std::chrono::seconds v) && {
    return std::move(set_ack_deadline(v));
  }

  SubscriptionBuilder& set_retain_acked_messages(bool v) & {
    proto_.set_retain_acked_messages(v);
    paths_.insert("retain_acked_messages");
    return *this;
  }
  SubscriptionBuilder&& set_retain_acked_messages(bool v) && {
    return std::move(set_retain_acked_messages(v));
  }

  template <typename Rep, typename Period>
  SubscriptionBuilder& set_message_retention_duration(
      std::chrono::duration<Rep, Period> d) & {
    *proto_.mutable_message_retention_duration() =
        google::cloud::internal::ToDurationProto(std::move(d));
    paths_.insert("message_retention_duration");
    return *this;
  }
  template <typename Rep, typename Period>
  SubscriptionBuilder&& set_message_retention_duration(
      std::chrono::duration<Rep, Period> d) && {
    return std::move(set_message_retention_duration(d));
  }

  SubscriptionBuilder& add_label(std::string const& key,
                                 std::string const& value) & {
    using value_type = protobuf::Map<std::string, std::string>::value_type;
    proto_.mutable_labels()->insert(value_type(key, value));
    paths_.insert("labels");
    return *this;
  }
  SubscriptionBuilder&& add_label(std::string const& key,
                                  std::string const& value) && {
    return std::move(add_label(key, value));
  }

  SubscriptionBuilder& set_labels(
      std::vector<std::pair<std::string, std::string>> new_labels) & {
    google::protobuf::Map<std::string, std::string> labels;
    for (auto& kv : new_labels) {
      labels[kv.first] = std::move(kv.second);
    }
    proto_.mutable_labels()->swap(labels);
    paths_.insert("labels");
    return *this;
  }
  SubscriptionBuilder&& set_labels(
      std::vector<std::pair<std::string, std::string>> new_labels) && {
    return std::move(set_labels(std::move(new_labels)));
  }

  SubscriptionBuilder& clear_labels() & {
    proto_.clear_labels();
    paths_.insert("labels");
    return *this;
  }
  SubscriptionBuilder&& clear_labels() && { return std::move(clear_labels()); }

  SubscriptionBuilder& enable_message_ordering(bool v) & {
    proto_.set_enable_message_ordering(v);
    paths_.insert("enable_message_ordering");
    return *this;
  }
  SubscriptionBuilder&& enable_message_ordering(bool v) && {
    return std::move(enable_message_ordering(v));
  }

  SubscriptionBuilder& set_expiration_policy(
      google::pubsub::v1::ExpirationPolicy v) & {
    *proto_.mutable_expiration_policy() = std::move(v);
    paths_.insert("expiration_policy");
    return *this;
  }
  SubscriptionBuilder&& set_expiration_policy(
      google::pubsub::v1::ExpirationPolicy v) && {
    return std::move(set_expiration_policy(std::move(v)));
  }

  SubscriptionBuilder& set_filter(std::string v) & {
    proto_.set_filter(std::move(v));
    paths_.insert("filter");
    return *this;
  }
  SubscriptionBuilder&& set_filter(std::string v) && {
    return std::move(set_filter(std::move(v)));
  }

  SubscriptionBuilder& set_dead_letter_policy(
      google::pubsub::v1::DeadLetterPolicy v) & {
    *proto_.mutable_dead_letter_policy() = std::move(v);
    paths_.insert("dead_letter_policy");
    return *this;
  }
  SubscriptionBuilder&& set_dead_letter_policy(
      google::pubsub::v1::DeadLetterPolicy v) && {
    return std::move(set_dead_letter_policy(std::move(v)));
  }

  SubscriptionBuilder& clear_dead_letter_policy() & {
    proto_.clear_dead_letter_policy();
    paths_.insert("dead_letter_policy");
    return *this;
  }
  SubscriptionBuilder&& clear_dead_letter_policy() && {
    return std::move(clear_dead_letter_policy());
  }

  SubscriptionBuilder& set_retry_policy(google::pubsub::v1::RetryPolicy v) & {
    *proto_.mutable_retry_policy() = std::move(v);
    paths_.insert("retry_policy");
    return *this;
  }
  SubscriptionBuilder&& set_retry_policy(google::pubsub::v1::RetryPolicy v) && {
    return std::move(set_retry_policy(std::move(v)));
  }

  SubscriptionBuilder& clear_retry_policy() & {
    proto_.clear_retry_policy();
    paths_.insert("retry_policy");
    return *this;
  }
  SubscriptionBuilder&& clear_retry_policy() && {
    return std::move(clear_retry_policy());
  }

  SubscriptionBuilder& enable_exactly_once_delivery(bool v) & {
    proto_.set_enable_exactly_once_delivery(v);
    paths_.insert("enable_exactly_once_delivery");
    return *this;
  }
  SubscriptionBuilder&& enable_exactly_once_delivery(bool v) && {
    return std::move(enable_exactly_once_delivery(v));
  }
  ///@}

  /**
   * Construct a `google::pubsub::v1::ExpirationPolicy` using a C++ duration.
   *
   * This is a convenience function to create the `set_expiration_policy()`
   * argument.
   */
  template <typename Rep, typename Period>
  static google::pubsub::v1::ExpirationPolicy MakeExpirationPolicy(
      std::chrono::duration<Rep, Period> d) {
    google::pubsub::v1::ExpirationPolicy result;
    *result.mutable_ttl() =
        google::cloud::internal::ToDurationProto(std::move(d));
    return result;
  }

  /**
   * Construct a `google::pubsub::v1::DeadLetterPolicy`.
   *
   * This is a convenience function to create the `set_dead_letter_policy()`
   * argument.
   */
  static google::pubsub::v1::DeadLetterPolicy MakeDeadLetterPolicy(
      Topic const& dead_letter_topic, std::int32_t max_delivery_attempts = 0) {
    google::pubsub::v1::DeadLetterPolicy result;
    result.set_dead_letter_topic(dead_letter_topic.FullName());
    result.set_max_delivery_attempts(max_delivery_attempts);
    return result;
  }

  /**
   * Construct a `google::pubsub::v1::RetryPolicy` using C++ durations.
   *
   * This is a convenience function to create the `set_retry_policy()`
   * argument.
   */
  template <typename Rep1, typename Period1, typename Rep2, typename Period2>
  static google::pubsub::v1::RetryPolicy MakeRetryPolicy(
      std::chrono::duration<Rep1, Period1> minimum_backoff,
      std::chrono::duration<Rep2, Period2> maximum_backoff) {
    google::pubsub::v1::RetryPolicy result;
    *result.mutable_minimum_backoff() =
        google::cloud::internal::ToDurationProto(minimum_backoff);
    *result.mutable_maximum_backoff() =
        google::cloud::internal::ToDurationProto(maximum_backoff);
    return result;
  }

 private:
  google::pubsub::v1::Subscription proto_;
  std::set<std::string> paths_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_BUILDER_H
