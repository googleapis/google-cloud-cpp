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

#include "subscription_mutation_builder.h"
#include "google/cloud/pubsub/topic_mutation_builder.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;

TEST(SubscriptionMutationBuilder, MakeOidcToken) {
  auto const actual =
      PushConfigBuilder::MakeOidcToken("test-account@example.com");
  google::pubsub::v1::PushConfig::OidcToken expected;
  std::string const text = R"pb(
    service_account_email: "test-account@example.com"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, MakeOidcTokenWithAudience) {
  auto const actual = PushConfigBuilder::MakeOidcToken(
      "test-account@example.com", "test-audience");
  google::pubsub::v1::PushConfig::OidcToken expected;
  std::string const text = R"pb(
    service_account_email: "test-account@example.com"
    audience: "test-audience"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, Empty) {
  auto const actual = PushConfigBuilder().BuildModifyPushConfig(
      Subscription("test-project", "test-subscription"));
  google::pubsub::v1::ModifyPushConfigRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, PushConfigEndpoint) {
  auto const actual = PushConfigBuilder()
                          .set_push_endpoint("https://endpoint.example.com")
                          .BuildModifyPushConfig(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::ModifyPushConfigRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
    push_config { push_endpoint: "https://endpoint.example.com" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, PushConfigAddAttribute) {
  auto const actual = PushConfigBuilder()
                          .set_push_endpoint("https://endpoint.example.com")
                          .add_attribute("key0", "label0")
                          .add_attribute("key1", "label1")
                          .BuildModifyPushConfig(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::ModifyPushConfigRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
    push_config {
      push_endpoint: "https://endpoint.example.com"
      attributes: { key: "key1" value: "label1" }
      attributes: { key: "key0" value: "label0" }
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, PushConfigSetAttributes) {
  auto const actual = PushConfigBuilder()
                          .set_push_endpoint("https://endpoint.example.com")
                          .add_attribute("key0", "label0")
                          .add_attribute("key1", "label1")
                          .set_attributes({{"key2", "label2"}})
                          .BuildModifyPushConfig(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::ModifyPushConfigRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
    push_config {
      push_endpoint: "https://endpoint.example.com"
      attributes: { key: "key2" value: "label2" }
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, PushConfigClearAttributes) {
  auto const actual = PushConfigBuilder()
                          .set_push_endpoint("https://endpoint.example.com")
                          .add_attribute("key0", "label0")
                          .add_attribute("key1", "label1")
                          .clear_attributes()
                          .add_attribute("key2", "label2")
                          .BuildModifyPushConfig(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::ModifyPushConfigRequest expected;
  std::string const text = R"pb(
    subscription: "projects/test-project/subscriptions/test-subscription"
    push_config {
      push_endpoint: "https://endpoint.example.com"
      attributes: { key: "key2" value: "label2" }
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, PushConfigSetAuthentication) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_push_config(
              PushConfigBuilder()
                  .set_push_endpoint("https://endpoint.example.com")
                  .set_authentication(PushConfigBuilder::MakeOidcToken(
                      "fake-service-account@example.com", "test-audience")))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      push_config {
        push_endpoint: "https://endpoint.example.com"
        oidc_token {
          service_account_email: "fake-service-account@example.com"
          audience: "test-audience"
        }
      }
    }
    update_mask {
      paths: "push_config.oidc_token"
      paths: "push_config.push_endpoint"
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, BuildUpdateSubscription) {
  auto const actual = SubscriptionMutationBuilder{}.BuildUpdateSubscription(
      Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, BuildCreateSubscription) {
  auto const actual = SubscriptionMutationBuilder{}.BuildCreateSubscription(
      Topic("test-project", "test-topic"),
      Subscription("test-project", "test-subscription"));
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    topic: "projects/test-project/topics/test-topic"
    name: "projects/test-project/subscriptions/test-subscription"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetPushConfigEmpty) {
  auto const actual = SubscriptionMutationBuilder{}
                          .set_push_config(PushConfigBuilder())
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
    }
    update_mask { paths: "push_config" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetPushConfigEndpoint) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_push_config(PushConfigBuilder("https://endpoint.example.com"))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      push_config { push_endpoint: "https://endpoint.example.com" }
    }
    update_mask { paths: "push_config.push_endpoint" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetPushConfigAddAttribute) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_push_config(PushConfigBuilder("https://endpoint.example.com")
                               .add_attribute("key0", "label0")
                               .add_attribute("key1", "label1"))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      push_config {
        push_endpoint: "https://endpoint.example.com"
        attributes: { key: "key1" value: "label1" }
        attributes: { key: "key0" value: "label0" }
      }
    }
    update_mask {
      paths: "push_config.attributes"
      paths: "push_config.push_endpoint"
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetPushConfigSetAttributes) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_push_config(PushConfigBuilder("https://endpoint.example.com")
                               .add_attribute("key0", "label0")
                               .add_attribute("key1", "label1")
                               .set_attributes({{"key2", "label2"}}))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      push_config {
        push_endpoint: "https://endpoint.example.com"
        attributes: { key: "key2" value: "label2" }
      }
    }
    update_mask {
      paths: "push_config.attributes"
      paths: "push_config.push_endpoint"
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetPushConfigSetAuthentication) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_push_config(
              PushConfigBuilder("https://endpoint.example.com")
                  .set_authentication(PushConfigBuilder::MakeOidcToken(
                      "fake-service-account@example.com", "test-audience")))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      push_config {
        push_endpoint: "https://endpoint.example.com"
        oidc_token {
          service_account_email: "fake-service-account@example.com"
          audience: "test-audience"
        }
      }
    }
    update_mask {
      paths: "push_config.oidc_token"
      paths: "push_config.push_endpoint"
    })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetAckDeadline) {
  auto const actual = SubscriptionMutationBuilder{}
                          .set_ack_deadline(std::chrono::seconds(600))
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      ack_deadline_seconds: 600
    }
    update_mask { paths: "ack_deadline_seconds" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetRetainAckedMessages) {
  auto const actual = SubscriptionMutationBuilder{}
                          .set_retain_acked_messages(true)
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      retain_acked_messages: true
    }
    update_mask { paths: "retain_acked_messages" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetMessageRetentionDuration) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_message_retention_duration(std::chrono::minutes(1) +
                                          std::chrono::seconds(2) +
                                          std::chrono::microseconds(3))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      message_retention_duration { seconds: 62 nanos: 3000 }
    }
    update_mask { paths: "message_retention_duration" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetPushConfig) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_push_config(
              PushConfigBuilder().set_push_endpoint("https://ep.example.com"))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      push_config { push_endpoint: "https://ep.example.com" }
    }
    update_mask { paths: "push_config.push_endpoint" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, AddLabels) {
  auto const actual = SubscriptionMutationBuilder{}
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      labels: { key: "key0", value: "label0" }
      labels: { key: "key1", value: "label1" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetLabels) {
  auto const actual = SubscriptionMutationBuilder{}
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .set_labels({{"key2", "label2"}})
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      labels: { key: "key2", value: "label2" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, ClearLabels) {
  auto const actual = SubscriptionMutationBuilder{}
                          .add_label("key0", "label0")
                          .clear_labels()
                          .add_label("key1", "label1")
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      labels: { key: "key1", value: "label1" }
    }
    update_mask { paths: "labels" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, EnableMessageOrdering) {
  auto const actual = SubscriptionMutationBuilder{}
                          .enable_message_ordering(true)
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      enable_message_ordering: true
    }
    update_mask { paths: "enable_message_ordering" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetExpirationPolicy) {
  auto const actual =
      SubscriptionMutationBuilder{}
          .set_expiration_policy(
              SubscriptionMutationBuilder::MakeExpirationPolicy(
                  std::chrono::hours(2) + std::chrono::nanoseconds(3)))
          .BuildUpdateSubscription(
              Subscription("test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      expiration_policy { ttl { seconds: 7200 nanos: 3 } }
    }
    update_mask { paths: "expiration_policy" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, SetDeadLetterPolicy) {
  auto const actual = SubscriptionMutationBuilder{}
                          .set_dead_letter_policy(
                              SubscriptionMutationBuilder::MakeDeadLetterPolicy(
                                  Topic("test-project", "dead-letter"), 3))
                          .BuildUpdateSubscription(Subscription(
                              "test-project", "test-subscription"));
  google::pubsub::v1::UpdateSubscriptionRequest expected;
  std::string const text = R"pb(
    subscription {
      name: "projects/test-project/subscriptions/test-subscription"
      dead_letter_policy {
        dead_letter_topic: "projects/test-project/topics/dead-letter"
        max_delivery_attempts: 3
      }
    }
    update_mask { paths: "dead_letter_policy" })pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

template <typename Duration>
void CheckMakeExpirationPolicy(Duration d,
                               std::string const& expected_as_text) {
  auto const actual = SubscriptionMutationBuilder::MakeExpirationPolicy(d);
  google::pubsub::v1::ExpirationPolicy expected;
  ASSERT_TRUE(TextFormat::ParseFromString(expected_as_text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(SubscriptionMutationBuilder, MakeExpirationPolicy) {
  using std::chrono::hours;
  using std::chrono::nanoseconds;
  using std::chrono::seconds;
  CheckMakeExpirationPolicy(seconds(0), R"pb(ttl { seconds: 0 })pb");
  CheckMakeExpirationPolicy(nanoseconds(1), R"pb(ttl { nanos: 1 })pb");
  CheckMakeExpirationPolicy(seconds(2) + nanoseconds(1),
                            R"pb(ttl { seconds: 2 nanos: 1 })pb");
  CheckMakeExpirationPolicy(hours(1), R"pb(ttl { seconds: 3600 })pb");
  CheckMakeExpirationPolicy(hours(1) + seconds(2) + nanoseconds(3),
                            R"pb(ttl { seconds: 3602 nanos: 3 })pb");
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
