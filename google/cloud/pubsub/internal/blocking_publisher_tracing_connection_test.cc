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

#include "google/cloud/pubsub/internal/blocking_publisher_tracing_connection.h"
#include "google/cloud/pubsub/blocking_publisher_connection.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/mocks/mock_blocking_publisher_connection.h"
#include "google/cloud/pubsub/topic.h"
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

auto constexpr kErrorCode = "ABORTED";

using ::google::cloud::pubsub::MessageBuilder;
using ::google::cloud::pubsub::Topic;
using ::google::cloud::pubsub_internal::MakeBlockingPublisherTracingConnection;
using ::google::cloud::pubsub_mocks::MockBlockingPublisherConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsProducer;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Not;

TEST(BlockingPublisherTracingConnectionTest, PublishSpanOnSuccess) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::BlockingPublisherConnection::PublishParams const&) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return StatusOr<std::string>("test-id-0");
      });
  auto connection = MakeBlockingPublisherTracingConnection(std::move(mock));

  Topic const topic("test-project", "test-topic");
  auto response =
      connection->Publish({topic, pubsub::MessageBuilder{}
                                      .SetData("test-data-0")
                                      .SetOrderingKey("ordering-key-0")
                                      .Build()});

  EXPECT_STATUS_OK(response);
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsProducer(),

          SpanNamed("test-topic create"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-topic"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(
                  "messaging.gcp_pubsub.message.ordering_key",
                  "ordering-key-0"),
              OTelAttribute<std::string>("gl-cpp.status_code", "OK"),
              OTelAttribute<std::int64_t>(/*sc::kMessagingMessageEnvelopeSize=*/
                                          "messaging.message.envelope.size",
                                          45),
              OTelAttribute<std::string>("messaging.message_id", "test-id-0"),
              OTelAttribute<std::string>(
                  sc::kCodeFunction, "pubsub::BlockingPublisher::Publish")))));
}

TEST(BlockingPublisherTracingConnectionTest, PublishSpanOnError) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::BlockingPublisherConnection::PublishParams const&) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return StatusOr<std::string>(internal::AbortedError("fail"));
      });
  auto connection = MakeBlockingPublisherTracingConnection(std::move(mock));
  Topic const topic("test-project", "test-topic");

  auto response =
      connection->Publish({topic, pubsub::MessageBuilder{}
                                      .SetData("test-data-0")
                                      .SetOrderingKey("ordering-key-0")
                                      .Build()});

  EXPECT_THAT(response, StatusIs(StatusCode::kAborted));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsProducer(),

          SpanNamed("test-topic create"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasAttributes(
              OTelAttribute<std::string>(sc::kMessagingSystem, "gcp_pubsub"),
              OTelAttribute<std::string>(sc::kMessagingDestinationName,
                                         "test-topic"),
              OTelAttribute<std::string>("gcp.project_id", "test-project"),
              OTelAttribute<std::string>(
                  "messaging.gcp_pubsub.message.ordering_key",
                  "ordering-key-0"),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode),
              OTelAttribute<std::int64_t>(/*sc::kMessagingMessageEnvelopeSize=*/
                                          "messaging.message.envelope.size",
                                          45)))));
}

TEST(BlockingPublisherTracingConnectionTest, PublishSpanOmitsOrderingKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::BlockingPublisherConnection::PublishParams const&) {
        return StatusOr<std::string>("test-id-0");
      });
  auto connection = MakeBlockingPublisherTracingConnection(std::move(mock));
  Topic const topic("test-project", "test-topic");

  auto response = connection->Publish({topic, pubsub::MessageBuilder{}
                                                  .SetData("test-data-0")
                                                  .SetOrderingKey("")
                                                  .Build()});

  EXPECT_STATUS_OK(response);
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsProducer(),
                  SpanNamed("test-topic create"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  Not(SpanHasAttributes(OTelAttribute<std::string>(
                      "messaging.gcp_pubsub.message.ordering_key", _))))));
}

TEST(BlockingPublisherTracingConnectionTest, OptionsNoSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, options).Times(1);
  auto connection = MakeBlockingPublisherTracingConnection(std::move(mock));

  connection->options();

  EXPECT_THAT(span_catcher->GetSpans(), testing::IsEmpty());
}

TEST(MakeBlockingPublisherTracingConnectionTest, CreateTracingConnection) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockBlockingPublisherConnection>();
  EXPECT_CALL(*mock, Publish).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return StatusOr<std::string>("test-id-0");
  });
  auto connection = MakeBlockingPublisherTracingConnection(std::move(mock));
  Topic const topic("test-project", "test-topic");
  connection->Publish({topic, MessageBuilder{}.SetData("test-data").Build()});
}

}  // namespace
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
