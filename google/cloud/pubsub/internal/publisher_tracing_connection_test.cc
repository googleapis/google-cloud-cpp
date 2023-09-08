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
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
namespace {

using ::google::cloud::pubsub::PublisherConnection;
using ::google::cloud::pubsub_internal::MakePublisherTracingConnection;
using ::google::cloud::pubsub_mocks::MockPublisherConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanCatcher;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;

class PublisherTracingConnectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    span_catcher_ = InstallSpanCatcher();
    mock_ = std::make_shared<MockPublisherConnection>();
  }

  std::shared_ptr<SpanCatcher> span_catcher_;
  std::shared_ptr<MockPublisherConnection> mock_;
};

TEST_F(PublisherTracingConnectionTest, FlushSpan) {
  EXPECT_CALL(*mock_, Flush).Times(1);
  auto connection = MakePublisherTracingConnection(std::move(mock_));

  connection->Flush(PublisherConnection::FlushParams{});

  EXPECT_THAT(
      span_catcher_->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("pubsub::Publisher::Flush"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(OTelAttribute<int>("gcloud.status_code", 0)))));
}

TEST_F(PublisherTracingConnectionTest, ResumePublishSpan) {
  EXPECT_CALL(*mock_, ResumePublish).Times(1);
  auto connection = MakePublisherTracingConnection(std::move(mock_));

  connection->ResumePublish(PublisherConnection::ResumePublishParams{});

  EXPECT_THAT(
      span_catcher_->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("pubsub::Publisher::ResumePublish"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(OTelAttribute<int>("gcloud.status_code", 0)))));
}

TEST(MakePublisherTracingConnectionTest, CreatesTracingConnection) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockPublisherConnection>();
  EXPECT_CALL(*mock, Flush).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
  });
  auto connection_ = MakePublisherTracingConnection(std::move(mock));

  connection_->Flush(PublisherConnection::FlushParams{});
}

}  // namespace
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
