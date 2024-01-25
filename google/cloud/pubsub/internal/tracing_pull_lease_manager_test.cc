// Copyright 2023 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/tracing_pull_lease_manager.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_pull_lease_manager.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/semantic_conventions.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_testing::MockPullLeaseManager;
using ::google::cloud::pubsub_testing::MockSubscriberStub;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Return;

pubsub::Subscription const kTestSubscription =
    pubsub::Subscription("test-project", "test-subscription");
auto constexpr kTestAckId = "test-ack-id";
auto constexpr kLeaseExtension = std::chrono::seconds(10);
auto const kCurrentTime = std::chrono::system_clock::now();

std::shared_ptr<PullLeaseManager> MakeTestPullLeaseManager(
    std::shared_ptr<MockPullLeaseManager> mock) {
  EXPECT_CALL(*mock, ack_id()).WillRepeatedly(Return(kTestAckId));
  EXPECT_CALL(*mock, subscription()).WillRepeatedly(Return(kTestSubscription));

  return MakeTracingPullLeaseManager(std::move(mock));
}

TEST(TracingPullLeaseManagerTest, ExtendSuccess) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullLeaseManager>();
  EXPECT_CALL(*mock, ExtendLease(_, _, _))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto manager = MakeTestPullLeaseManager(std::move(mock));
  auto stub = std::make_shared<MockSubscriberStub>();

  auto status = manager->ExtendLease(stub, kCurrentTime, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                         SpanNamed("test-subscription modack"))));
}

TEST(TracingPullLeaseManagerTest, ExtendError) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullLeaseManager>();
  EXPECT_CALL(*mock, ExtendLease(_, _, _))
      .WillOnce(Return(ByMove(
          make_ready_future(Status{StatusCode::kPermissionDenied, "uh-oh"}))));
  auto manager = MakeTestPullLeaseManager(std::move(mock));
  auto stub = std::make_shared<MockSubscriberStub>();

  auto status = manager->ExtendLease(stub, kCurrentTime, kLeaseExtension);
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied, "uh-oh"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanWithStatus(opentelemetry::trace::StatusCode::kError),
                     SpanNamed("test-subscription modack"))));
}

TEST(TracingPullLeaseManagerTest, ExtendAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullLeaseManager>();
  EXPECT_CALL(*mock, ExtendLease(_, _, _))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto manager = MakeTestPullLeaseManager(std::move(mock));
  auto stub = std::make_shared<MockSubscriberStub>();

  auto status = manager->ExtendLease(stub, kCurrentTime, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription modack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription modack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingOperation, "modack")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription modack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kCodeFunction,
                                 "pubsub::PullLeaseManager::ExtendLease")))));
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanNamed("test-subscription modack"),
          SpanHasAttributes(OTelAttribute<std::int32_t>(
              "messaging.gcp_pubsub.message.ack_deadline_seconds", 10)))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription modack"),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "messaging.gcp_pubsub.message.ack_id", "test-ack-id")))));
  EXPECT_THAT(spans,
              Contains(AllOf(
                  SpanNamed("test-subscription modack"),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      sc::kMessagingDestinationName, "test-subscription")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-subscription modack"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "gcp.project_id", "test-project")))));
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2

using ::google::cloud::testing_util::SpanLinksSizeIs;

TEST(TracingPullLeaseManagerTest, ExtendAddsLink) {
  auto span_catcher = InstallSpanCatcher();
  auto consumer_span = internal::MakeSpan("receive");
  auto scope = internal::OTelScope(consumer_span);
  auto mock = std::make_unique<MockPullLeaseManager>();
  EXPECT_CALL(*mock, ExtendLease(_, _, _))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto manager = MakeTestPullLeaseManager(std::move(mock));
  auto stub = std::make_shared<MockSubscriberStub>();
  consumer_span->End();

  auto status = manager->ExtendLease(stub, kCurrentTime, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test-subscription modack"),
                                    SpanLinksSizeIs(1))));
}

#else

TEST(TracingPullLeaseManagerTest, ExtendAddsSpanIdAndTraceIdAttribute) {
  auto span_catcher = InstallSpanCatcher();
  auto consumer_span = internal::MakeSpan("receive");
  auto scope = internal::OTelScope(consumer_span);
  auto mock = std::make_unique<MockPullLeaseManager>();
  EXPECT_CALL(*mock, ExtendLease(_, _, _))
      .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  auto manager = MakeTestPullLeaseManager(std::move(mock));
  auto stub = std::make_shared<MockSubscriberStub>();
  consumer_span->End();

  auto status = manager->ExtendLease(stub, kCurrentTime, kLeaseExtension);
  EXPECT_STATUS_OK(status.get());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanNamed("test-subscription modack"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gcp_pubsub.receive.trace_id", _),
              OTelAttribute<std::string>("gcp_pubsub.receive.span_id", _)))));
}

#endif

TEST(TracingPullLeaseManagerTest, AckIdNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullLeaseManager>();
  auto manager = MakeTestPullLeaseManager(std::move(mock));

  EXPECT_EQ(kTestAckId, manager->ack_id());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

TEST(TracingPullLeaseManagerTest, SubscriptionNoSpans) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockPullLeaseManager>();
  auto manager = MakeTestPullLeaseManager(std::move(mock));

  EXPECT_EQ(kTestSubscription, manager->subscription());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
