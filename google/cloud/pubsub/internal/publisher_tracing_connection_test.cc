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

#include "google/cloud/pubsub/internal/publisher_tracing_connection.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/future_generic.h"
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

using ::google::cloud::pubsub::PublisherConnection;
using ::google::cloud::pubsub::Topic;
using ::google::cloud::pubsub_internal::MakePublisherTracingConnection;
using ::google::cloud::pubsub_mocks::MockPublisherConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanKindIsProducer;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::Pair;
using ::testing::StartsWith;

TEST(PublisherTracingConnectionTest, PublishSpanOnSuccess) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::PublisherConnection::PublishParams const&) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(StatusOr<std::string>("test-id-0"));
      });
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  auto response = connection
                      ->Publish({pubsub::MessageBuilder{}
                                     .SetData("test-data-0")
                                     .SetOrderingKey("ordering-key-0")
                                     .Build()})
                      .get();

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
              OTelAttribute<std::string>(
                  /*sc::kMessagingOperationType=*/"messaging.operation.type",
                  "create"),
              OTelAttribute<std::string>(sc::kMessagingMessageId, "test-id-0"),
              OTelAttribute<std::string>(
                  sc::kCodeFunction,
                  "pubsub::PublisherConnection::Publish")))));
}

TEST(PublisherTracingConnectionTest, PublishSpanOnError) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::PublisherConnection::PublishParams const&) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(
            StatusOr<std::string>(internal::AbortedError("fail")));
      });
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  auto response = connection
                      ->Publish({pubsub::MessageBuilder{}
                                     .SetData("test-data-0")
                                     .SetOrderingKey("ordering-key-0")
                                     .Build()})
                      .get();

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
              OTelAttribute<std::string>(
                  /*sc::kMessagingOperationType=*/"messaging.operation.type",
                  "create"),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode),
              OTelAttribute<std::int64_t>(/*sc::kMessagingMessageEnvelopeSize=*/
                                          "messaging.message.envelope.size",
                                          45)))));
}

TEST(PublisherTracingConnectionTest, PublishInjectsTraceContext) {
  auto mock = std::make_shared<MockPublisherConnection>();
  // Need to install the span catcher so ThereIsAnActiveSpan() detects a span.
  auto span_catcher = InstallSpanCatcher();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::PublisherConnection::PublishParams const& p) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        // We need to test the trace context has been injected here, since the
        // connection moves the message to the child connection.
        EXPECT_THAT(p.message.attributes(),
                    Contains(Pair(StartsWith("googclient_"), _)));
        return make_ready_future(StatusOr<std::string>("test-id-0"));
      });
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  auto message = pubsub::MessageBuilder{}
                     .SetData("test-data-0")
                     .SetOrderingKey("ordering-key-0")
                     .Build();
  auto response = connection->Publish({message}).get();
}

TEST(PublisherTracingConnectionTest, PublishSpanOmitsOrderingKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::PublisherConnection::PublishParams const&) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return make_ready_future(StatusOr<std::string>("test-id-0"));
      });
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  auto response = connection
                      ->Publish({pubsub::MessageBuilder{}
                                     .SetData("test-data-0")
                                     .SetOrderingKey("")
                                     .Build()})
                      .get();

  EXPECT_STATUS_OK(response);
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsProducer(),
                  SpanNamed("test-topic create"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  Not(SpanHasAttributes(OTelAttribute<std::string>(
                      "messaging.gcp_pubsub.message.ordering_key", _))))));
}

TEST(PublisherTracingConnectionTest, FlushSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Flush).Times(1);
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  connection->Flush(PublisherConnection::FlushParams{});

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanNamed("pubsub::Publisher::Flush"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                        SpanHasAttributes(OTelAttribute<std::string>(
                            "gl-cpp.status_code", "OK")))));
}

TEST(PublisherTracingConnectionTest, ResumePublishSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, ResumePublish).Times(1);
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  connection->ResumePublish(PublisherConnection::ResumePublishParams{});

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanNamed("pubsub::Publisher::ResumePublish"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                        SpanHasAttributes(OTelAttribute<std::string>(
                            "gl-cpp.status_code", "OK")))));
}

TEST(MakePublisherTracingConnectionTest, CreateTracingConnection) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
  });
  auto connection = MakePublisherTracingConnection(
      Topic("test-project", "test-topic"), std::move(mock));

  connection->Flush(PublisherConnection::FlushParams{});
}

}  // namespace
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
