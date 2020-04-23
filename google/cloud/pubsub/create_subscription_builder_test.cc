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

#include "create_subscription_builder.h"
#include "google/cloud/pubsub/create_topic_builder.h"
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

TEST(CreateSubscriptionBuilder, MakeOidcToken) {
  auto const actual =
      PushConfigBuilder::MakeOidcToken("test-account@example.com");
  google::pubsub::v1::PushConfig::OidcToken expected;
  std::string const text = R"pb(
    service_account_email: "test-account@example.com"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, MakeOidcTokenWithAudience) {
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

TEST(CreateSubscriptionBuilder, PushConfigBasic) {
  auto const actual =
      PushConfigBuilder("https://endpoint.example.com").as_proto();
  google::pubsub::v1::PushConfig expected;
  std::string const text = R"pb(
    push_endpoint: "https://endpoint.example.com"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, PushConfigAddAttribute) {
  auto const actual = PushConfigBuilder("https://endpoint.example.com")
                          .add_attribute("key0", "label0")
                          .add_attribute("key1", "label1")
                          .as_proto();
  google::pubsub::v1::PushConfig expected;
  std::string const text = R"pb(
    push_endpoint: "https://endpoint.example.com"
    attributes: { key: "key1" value: "label1" }
    attributes: { key: "key0" value: "label0" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, PushConfigSetAttributes) {
  auto const actual = PushConfigBuilder("https://endpoint.example.com")
                          .add_attribute("key0", "label0")
                          .add_attribute("key1", "label1")
                          .set_attributes({{"key2", "label2"}})
                          .as_proto();
  google::pubsub::v1::PushConfig expected;
  std::string const text = R"pb(
    push_endpoint: "https://endpoint.example.com"
    attributes: { key: "key2" value: "label2" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, PushConfigSetAuthentication) {
  auto const actual =
      PushConfigBuilder("https://endpoint.example.com")
          .set_authentication(PushConfigBuilder::MakeOidcToken(
              "fake-service-account@example.com", "test-audience"))
          .as_proto();
  google::pubsub::v1::PushConfig expected;
  std::string const text = R"pb(
    push_endpoint: "https://endpoint.example.com"
    oidc_token {
      service_account_email: "fake-service-account@example.com"
      audience: "test-audience"
    }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, Basic) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetAckDeadline) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .set_ack_deadline(std::chrono::seconds(600))
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    ack_deadline_seconds: 600
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetRetainAckedMessages) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .set_retain_acked_messages(true)
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    retain_acked_messages: true
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetMessageRetentionDuration) {
  auto const actual =
      CreateSubscriptionBuilder(
          Subscription("test-project", "test-subscription"),
          Topic("test-project", "test-topic"))
          .set_message_retention_duration(std::chrono::minutes(1) +
                                          std::chrono::seconds(2) +
                                          std::chrono::microseconds(3))
          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    message_retention_duration { seconds: 62 nanos: 3000 }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetPushConfig) {
  auto const actual =
      CreateSubscriptionBuilder(
          Subscription("test-project", "test-subscription"),
          Topic("test-project", "test-topic"))
          .set_push_config(
              PushConfigBuilder("https://ep.example.com").as_proto())
          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    push_config { push_endpoint: "https://ep.example.com" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, AddLabels) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    labels: { key: "key0", value: "label0" }
    labels: { key: "key1", value: "label1" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetLabels) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .add_label("key1", "label1")
                          .set_labels({{"key2", "label2"}})
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    labels: { key: "key2", value: "label2" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, ClearLabels) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .add_label("key0", "label0")
                          .clear_labels()
                          .add_label("key1", "label1")
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    labels: { key: "key1", value: "label1" }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, EnableMessageOrdering) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .enable_message_ordering(true)
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    enable_message_ordering: true
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetExpirationPolicy) {
  auto const actual =
      CreateSubscriptionBuilder(
          Subscription("test-project", "test-subscription"),
          Topic("test-project", "test-topic"))
          .set_expiration_policy(
              CreateSubscriptionBuilder::MakeExpirationPolicy(
                  std::chrono::hours(2) + std::chrono::nanoseconds(3)))
          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    expiration_policy { ttl { seconds: 7200 nanos: 3 } }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, SetDeadLetterPolicy) {
  auto const actual = CreateSubscriptionBuilder(
                          Subscription("test-project", "test-subscription"),
                          Topic("test-project", "test-topic"))
                          .set_dead_letter_policy(
                              CreateSubscriptionBuilder::MakeDeadLetterPolicy(
                                  Topic("test-project", "dead-letter"), 3))
                          .as_proto();
  google::pubsub::v1::Subscription expected;
  std::string const text = R"pb(
    name: "projects/test-project/subscriptions/test-subscription"
    topic: "projects/test-project/topics/test-topic"
    dead_letter_policy {
      dead_letter_topic: "projects/test-project/topics/dead-letter"
      max_delivery_attempts: 3
    }
  )pb";
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

template <typename Duration>
void CheckMakeExpirationPolicy(Duration d,
                               std::string const& expected_as_text) {
  auto const actual = CreateSubscriptionBuilder::MakeExpirationPolicy(d);
  google::pubsub::v1::ExpirationPolicy expected;
  ASSERT_TRUE(TextFormat::ParseFromString(expected_as_text, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(CreateSubscriptionBuilder, MakeExpirationPolicy) {
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
