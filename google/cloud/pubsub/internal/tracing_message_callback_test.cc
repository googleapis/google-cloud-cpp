// Copyright 2024 Google LLC
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

#include "google/cloud/pubsub/internal/tracing_message_callback.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_exactly_once_ack_handler_impl.h"
#include "google/cloud/pubsub/testing/mock_message_callback.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/semantic_conventions.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsInternal;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithParent;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Return;

pubsub::Subscription TestSubscription() {
  return pubsub::Subscription("test-project", "test-sub");
}

std::shared_ptr<MessageCallback> MakeTestMessageCallback(
    std::shared_ptr<MessageCallback> mock) {
  return MakeTracingMessageCallback(
      std::move(mock),
      Options{}.set<pubsub::SubscriptionOption>(TestSubscription()));
}

TEST(TracingMessageCallback, UserCallback) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<pubsub_testing::MockMessageCallback>();
  EXPECT_CALL(*mock, user_callback).Times(1);
  auto message_callback = MakeTestMessageCallback(std::move(mock));
  auto mock_ack_handler =
      std::make_unique<pubsub_testing::MockExactlyOnceAckHandlerImpl>();
  auto span = internal::MakeSpan("test-sub subscribe");
  MessageCallback::MessageAndHandler m{pubsub::MessageBuilder().Build(),
                                       std::move(mock_ack_handler),
                                       "ack-id",
                                       {span}};
  message_callback->user_callback(std::move(m));
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                            SpanNamed("test-sub process"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                sc::kMessagingSystem, "gcp_pubsub")),
                            SpanWithParent(span))));
}

TEST(TracingMessageCallback, AddTracingAckHandler) {
  namespace sc = opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock_handler =
      std::make_unique<pubsub_testing::MockExactlyOnceAckHandlerImpl>();
  EXPECT_CALL(*mock_handler, ack_id).WillRepeatedly(Return("ack-id"));
  EXPECT_CALL(*mock_handler, subscription).WillOnce(Return(TestSubscription()));
  EXPECT_CALL(*mock_handler, delivery_attempt).WillOnce(Return(0));
  EXPECT_CALL(*mock_handler, ack()).WillOnce([&]() {
    ThereIsAnActiveSpan();
    return make_ready_future(Status{});
  });
  auto mock = std::make_shared<pubsub_testing::MockMessageCallback>();
  auto span = internal::MakeSpan("test-sub subscribe");
  MessageCallback::MessageAndHandler m{pubsub::MessageBuilder().Build(),
                                       std::move(mock_handler),
                                       "ack-id",
                                       {span}};
  EXPECT_CALL(*mock, user_callback)
      .WillOnce([&](MessageCallback::MessageAndHandler m) {
        EXPECT_EQ(m.ack_handler->ack_id(), "ack-id");
        auto f = m.ack_handler->ack();
        f.get();
      });
  auto message_callback = MakeTestMessageCallback(std::move(mock));

  message_callback->user_callback(std::move(m));
  span->End();

  auto spans = span_catcher->GetSpans();
  // Contains 3 spans: 1) created in the test, 2) generated by the callback, 3)
  // generated by the ack handler
  EXPECT_THAT(spans,
              UnorderedElementsAre(
                  SpanNamed("test-sub subscribe"),
                  AllOf(SpanHasInstrumentationScope(), SpanKindIsInternal(),
                        SpanNamed("test-sub process"),
                        SpanHasAttributes(OTelAttribute<std::string>(
                            sc::kMessagingSystem, "gcp_pubsub")),
                        SpanWithParent(span)),
                  SpanNamed("test-sub ack")));
}

#else  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

using ::testing::IsNull;
using ::testing::Not;

TEST(TracingMessageCallback,
     VerifyMessageCallbackIsNotNullWhenOTelIsNotCompiled) {
  auto mock = std::make_shared<pubsub_testing::MockMessageCallback>();
  EXPECT_CALL(*mock, user_callback).Times(1);
  auto message_callback = MakeTracingMessageCallback(
      std::move(mock), Options{}.set<pubsub::SubscriptionOption>(
                           pubsub::Subscription("test-project", "test-sub")));

  EXPECT_THAT(message_callback, Not(IsNull()));

  message_callback->user_callback(MessageCallback::MessageAndHandler{
      pubsub::MessageBuilder().Build(),
      std::make_unique<pubsub_testing::MockExactlyOnceAckHandlerImpl>(),
      "ack-id",
      {}});
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
