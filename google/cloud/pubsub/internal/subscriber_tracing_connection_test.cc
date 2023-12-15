// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/subscriber_tracing_connection.h"
#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/mocks/mock_exactly_once_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_pull_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_subscriber_connection.h"
#include "google/cloud/pubsub/pull_ack_handler.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
namespace {

using ::google::cloud::pubsub::SubscriberConnection;
using ::google::cloud::pubsub_internal::MakeSubscriberTracingConnection;
using ::google::cloud::pubsub_mocks::MockPullAckHandler;
using ::google::cloud::pubsub_mocks::MockSubscriberConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsConsumer;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;

pubsub::Subscription TestSubscription() {
  return pubsub::Subscription("test-project", "test-subscription");
}

pubsub::PullResponse MakePullResponse() {
  auto mock = std::make_unique<MockPullAckHandler>();
  EXPECT_CALL(*mock, nack())
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  return pubsub::PullResponse{
      pubsub::PullAckHandler(std::move(mock)),
      pubsub::MessageBuilder{}.SetData("test-data-0").Build()};
}

TEST(SubscriberTracingConnectionTest, PullOnSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Pull).WillOnce([&]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return MakePullResponse();
  });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));
  google::cloud::internal::OptionsSpan span(
      connection->options().set<pubsub::SubscriptionOption>(
          TestSubscription()));

  auto response = connection->Pull();
  EXPECT_STATUS_OK(response);
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                  SpanNamed("test-subscription receive"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk))));
}

TEST(SubscriberTracingConnectionTest, PullOnError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Pull).WillOnce([&]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return StatusOr<pubsub::PullResponse>(internal::AbortedError("fail"));
  });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));
  google::cloud::internal::OptionsSpan span(
      connection->options().set<pubsub::SubscriptionOption>(
          TestSubscription()));

  auto response = connection->Pull();
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted));
  EXPECT_THAT(span_catcher->GetSpans(),
              Contains(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsConsumer(),
                  SpanNamed("test-subscription receive"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError))));
}

TEST(SubscriberTracingConnectionTest, PullAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Pull).WillOnce([&]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return MakePullResponse();
  });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));
  google::cloud::internal::OptionsSpan span(
      connection->options().set<pubsub::SubscriptionOption>(
          TestSubscription()));

  auto response = connection->Pull();
  EXPECT_STATUS_OK(response);
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kCodeFunction,
                                 "pubsub::SubscriberConnection::Pull")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingOperation, "receive")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingDestinationTemplate,
                                 TestSubscription().FullName())))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingDestinationTemplate,
                                 TestSubscription().FullName())))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingMessageId, _)))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription receive"),
                             SpanHasAttributes(OTelAttribute<std::int64_t>(
                                 /*sc::kMessagingMessageEnvelopeSize=*/
                                 "messaging.message.envelope.size", 31)))));
}

TEST(SubscriberTracingConnectionTest, PullSetsOrderingKeyAttributeIfExists) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Pull).WillOnce([&]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    auto mock = std::make_unique<MockPullAckHandler>();
    EXPECT_CALL(*mock, nack())
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    return pubsub::PullResponse{pubsub::PullAckHandler(std::move(mock)),
                                pubsub::MessageBuilder{}
                                    .SetData("test-data-0")
                                    .SetOrderingKey("a")
                                    .Build()};
  });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));
  google::cloud::internal::OptionsSpan span(
      connection->options().set<pubsub::SubscriptionOption>(
          TestSubscription()));

  auto response = connection->Pull();
  EXPECT_STATUS_OK(response);
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanNamed("test-subscription receive"),
                     SpanHasAttributes(OTelAttribute<std::string>(
                         "messaging.gcp_pubsub.message.ordering_key", "a")))));
}

TEST(SubscriberTracingConnectionTest, Subscribe) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, Subscribe)
      .WillOnce([&](SubscriberConnection::SubscribeParams const&) {
        return make_ready_future(Status{});
      });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));

  auto handler = [&](pubsub::Message const&, pubsub::AckHandler const&) {};
  auto status = connection->Subscribe({handler}).get();
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

TEST(SubscriberTracingConnectionTest, ExactlyOnceSubscribe) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, ExactlyOnceSubscribe)
      .WillOnce([&](SubscriberConnection::ExactlyOnceSubscribeParams const&) {
        return make_ready_future(Status{});
      });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));

  auto handler = [&](pubsub::Message const&,
                     pubsub::ExactlyOnceAckHandler const&) {};
  auto status = connection->ExactlyOnceSubscribe({handler}).get();
  ASSERT_STATUS_OK(status);
  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

TEST(SubscriberTracingConnectionTest, options) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockSubscriberConnection>();
  EXPECT_CALL(*mock, options).WillOnce([&]() {
    EXPECT_FALSE(ThereIsAnActiveSpan());
    return Options{};
  });
  auto connection = MakeSubscriberTracingConnection(std::move(mock));

  auto response = connection->options();
  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

}  // namespace
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
