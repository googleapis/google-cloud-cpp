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

#include "google/cloud/pubsub/internal/batching_publisher_tracing_connection.h"
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
namespace {

using ::google::cloud::pubsub::PublisherConnection;
using ::google::cloud::pubsub_internal::MakeBatchingPublisherTracingConnection;
using ::google::cloud::pubsub_mocks::MockPublisherConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::SizeIs;

TEST(BatchingPublisherTracingConnectionTest, PublishSpan) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::PublisherConnection::PublishParams const&) {
        EXPECT_FALSE(ThereIsAnActiveSpan());
        return google::cloud::make_ready_future(
            google::cloud::StatusOr<std::string>("test-id-0"));
      });
  auto connection = MakeBatchingPublisherTracingConnection(std::move(mock));

  auto response = connection
                      ->Publish({pubsub::MessageBuilder{}
                                     .SetData("test-data-0")
                                     .SetOrderingKey("ordering-key-0")
                                     .Build()})
                      .get();

  EXPECT_STATUS_OK(response);
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(
          AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                SpanNamed("publisher batching"),
                SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                SpanHasAttributes(
                    OTelAttribute<std::string>(
                        sc::kCodeFunction,
                        "pubsub::BatchingPublisherConnection::Publish"),
                    OTelAttribute<std::string>("gl-cpp.status_code", "OK")))));
}

TEST(BatchingPublisherTracingConnectionTest, FlushSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_FALSE(ThereIsAnActiveSpan());
  });
  auto connection = MakeBatchingPublisherTracingConnection(std::move(mock));

  connection->Flush(PublisherConnection::FlushParams{});

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                        SpanNamed("pubsub::BatchingPublisherConnection::Flush"),
                        SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                        SpanHasAttributes(OTelAttribute<std::string>(
                            "gl-cpp.status_code", "OK")))));
}

TEST(BatchingPublisherTracingConnectionTest, ResumePublishSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, ResumePublish).WillOnce([] {
    EXPECT_FALSE(ThereIsAnActiveSpan());
  });

  auto connection = MakeBatchingPublisherTracingConnection(std::move(mock));

  connection->ResumePublish(PublisherConnection::ResumePublishParams{});

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(
          AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                SpanNamed("pubsub::BatchingPublisherConnection::ResumePublish"),
                SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                SpanHasAttributes(
                    OTelAttribute<std::string>("gl-cpp.status_code", "OK")))));
}

TEST(MakeBatchingPublisherTracingConnectionTest, CreateTracingConnection) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_FALSE(ThereIsAnActiveSpan());
  });
  auto connection = MakeBatchingPublisherTracingConnection(std::move(mock));

  connection->Flush(PublisherConnection::FlushParams{});
  EXPECT_THAT(span_catcher->GetSpans(), SizeIs(1));
}

}  // namespace
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
