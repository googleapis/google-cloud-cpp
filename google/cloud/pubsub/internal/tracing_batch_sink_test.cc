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

#include "google/cloud/pubsub/internal/tracing_batch_sink.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/testing/mock_batch_sink.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/common_options.h"
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

using ::google::cloud::internal::MakeSpan;
using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::LinkHasSpanContext;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasEvents;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanIsRoot;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanLinksSizeIs;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;

namespace {

auto constexpr kDefaultMaxLinks = 128;
auto constexpr kDefaultEndpoint = "endpoint";

void EndSpans(std::vector<opentelemetry::nostd::shared_ptr<
                  opentelemetry::trace::Span>> const& spans) {
  for (auto const& span : spans) {
    span->End();
  }
}

/// Creates @p n spans.
auto CreateSpans(int n) {
  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      spans(n);
  std::generate(spans.begin(), spans.end(), [i = 0]() mutable {
    return MakeSpan("test span " + std::to_string(i++));
  });
  return spans;
}

/// Saves a message for each span and ends @p spans. If @p end_spans is true, it
/// will end the spans.
void AddMessages(
    std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
        spans,
    std::shared_ptr<BatchSink> const& batch_sink, bool end_spans = true) {
  for (size_t i = 0; i < spans.size(); i++) {
    auto message =
        pubsub::MessageBuilder().SetData("test" + std::to_string(i)).Build();
    auto scope = opentelemetry::trace::Scope(spans[i]);
    batch_sink->AddMessage(message);
    if (end_spans) {
      spans[i]->End();
    }
  }
}

/// Makes test options.
auto MakeTestOptions(size_t max_otel_link_count = kDefaultMaxLinks) {
  Options options;
  options.set<pubsub::MaxOtelLinkCountOption>(max_otel_link_count);
  options.set<EndpointOption>(kDefaultEndpoint);
  return options;
}

pubsub::Topic TestTopic() {
  return pubsub::Topic("test-project", "test-topic");
}

std::shared_ptr<BatchSink> MakeTestBatchSink(std::shared_ptr<BatchSink> mock,
                                             Options options = {}) {
  return MakeTracingBatchSink(
      TestTopic(), std::move(mock),
      internal::MergeOptions(std::move(options), MakeTestOptions()));
}

google::pubsub::v1::PublishRequest MakeRequest(int n) {
  google::pubsub::v1::PublishRequest request;
  request.set_topic(TestTopic().FullName());
  for (int i = 0; i != n; ++i) {
    request.add_messages()->set_message_id("message-" + std::to_string(i));
  }
  return request;
}

google::pubsub::v1::PublishResponse MakeResponse(
    google::pubsub::v1::PublishRequest const& request) {
  google::pubsub::v1::PublishResponse response;
  for (auto const& m : request.messages()) {
    response.add_message_ids("id-" + m.message_id());
  }
  return response;
}

TEST(TracingBatchSink, AddMessageAddsEvent) {
  auto span_catcher = InstallSpanCatcher();
  auto span = MakeSpan("test span");
  opentelemetry::trace::Scope scope(span);
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto message = pubsub::MessageBuilder().SetData("test").Build();

  batch_sink->AddMessage(message);

  span->End();

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanHasEvents(EventNamed("gl-cpp.added_to_batch")))));
}

TEST(TracingBatchSink, AsyncPublish) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test span");
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });

  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto initial_spans = {message_span};
  AddMessages(initial_spans, batch_sink);

  auto response = batch_sink->AsyncPublish(MakeRequest(1)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                             SpanNamed("test-topic publish"),
                             SpanHasAttributes(OTelAttribute<std::int64_t>(
                                 sc::kMessagingBatchMessageCount, 1)),
                             SpanHasLinks(AllOf(LinkHasSpanContext(
                                 message_span->GetContext()))))));
}

TEST(TracingBatchSink, PublishSpanHasAttributes) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test span");
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink = MakeTestBatchSink(std::move(mock));
  auto initial_spans = {message_span};
  AddMessages(initial_spans, batch_sink);

  auto response = batch_sink->AsyncPublish(MakeRequest(1)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-topic publish"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kThreadId, _)))));
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test-topic publish"),
                         SpanHasAttributes(OTelAttribute<std::string>(
                             sc::kCodeFunction, "BatchSink::AsyncPublish")))));
  EXPECT_THAT(
      spans, Contains(AllOf(
                 SpanNamed("test-topic publish"),
                 SpanHasAttributes(OTelAttribute<std::string>(
                     /*sc::kMessagingOperationType=*/"messaging.operation.type",
                     "publish")))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-topic publish"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 sc::kMessagingSystem, "gcp_pubsub")))));
  EXPECT_THAT(
      spans, Contains(AllOf(SpanNamed("test-topic publish"),
                            SpanHasAttributes(OTelAttribute<std::string>(
                                "gcp.project_id", TestTopic().project_id())))));
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-topic publish"),
                             SpanHasAttributes(OTelAttribute<std::string>(
                                 "server.address", kDefaultEndpoint)))));
  EXPECT_THAT(
      spans, Contains(AllOf(
                 SpanNamed("test-topic publish"),
                 SpanHasAttributes(OTelAttribute<std::string>(
                     sc::kMessagingDestinationName, TestTopic().topic_id())))));
}

#if OPENTELEMETRY_VERSION_MAJOR >= 2 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 13)
TEST(TracingBatchSink, PublishSpanIsRoot) {
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test span");
  auto scope = opentelemetry::trace::Scope(message_span);
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink = MakeTestBatchSink(std::move(mock));
  auto initial_spans = {message_span};
  AddMessages(initial_spans, batch_sink);

  auto response = batch_sink->AsyncPublish(MakeRequest(1)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("test-topic publish"), SpanIsRoot())));
}
#endif

TEST(TracingBatchSink, AsyncPublishOnlyIncludeSampledLink) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  // Create span before the span catcher so it is not sampled.
  auto unsampled_span = MakeSpan("test skipped span");
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test span");
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_)).Times(2);
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(2)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto initial_spans = {message_span, unsampled_span};
  AddMessages(initial_spans, batch_sink);
  auto response = batch_sink->AsyncPublish(MakeRequest(2)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                             SpanNamed("test-topic publish"),
                             SpanHasAttributes(OTelAttribute<std::int64_t>(
                                 sc::kMessagingBatchMessageCount, 2)),
                             SpanLinksAre(AllOf(LinkHasSpanContext(
                                 message_span->GetContext()))))));
}

TEST(TracingBatchSink, AsyncPublishSmallBatch) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto message_span1 = MakeSpan("test span 1");
  auto message_span2 = MakeSpan("test span 2");
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_)).Times(2);
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(2)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink = MakeTestBatchSink(std::move(mock));
  auto initial_spans = {message_span1, message_span2};
  AddMessages(initial_spans, batch_sink);
  auto response = batch_sink->AsyncPublish(MakeRequest(2)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("test-topic publish"),
          SpanHasAttributes(
              OTelAttribute<std::int64_t>(sc::kMessagingBatchMessageCount, 2)),
          SpanHasLinks(LinkHasSpanContext(message_span1->GetContext()),
                       LinkHasSpanContext(message_span2->GetContext())))));
}

TEST(TracingBatchSink, AsyncPublishBatchWithOtelLimit) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_)).Times(kDefaultMaxLinks);
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(kDefaultMaxLinks)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto span_catcher = InstallSpanCatcher();
  auto batch_sink = MakeTestBatchSink(std::move(mock));
  AddMessages(CreateSpans(kDefaultMaxLinks), batch_sink);
  auto response = batch_sink->AsyncPublish(MakeRequest(kDefaultMaxLinks)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                     SpanNamed("test-topic publish"),
                     SpanHasAttributes(OTelAttribute<std::int64_t>(
                         sc::kMessagingBatchMessageCount, kDefaultMaxLinks)),
                     SpanLinksSizeIs(kDefaultMaxLinks))));
}

TEST(TracingBatchSink, AsyncPublishLargeBatch) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto const batch_size = kDefaultMaxLinks + 1;
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_)).Times(batch_size);
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(kDefaultMaxLinks + 1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto span_catcher = InstallSpanCatcher();
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  AddMessages(CreateSpans(batch_size), batch_sink);
  auto response = batch_sink->AsyncPublish(MakeRequest(batch_size)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test-topic publish"),
                         SpanHasAttributes(OTelAttribute<std::int64_t>(
                             sc::kMessagingBatchMessageCount, batch_size)))));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("publish #0"), SpanKindIsClient(),
                                    SpanLinksSizeIs(kDefaultMaxLinks))));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("publish #1"), SpanKindIsClient(),
                                    SpanLinksSizeIs(1))));
}

TEST(TracingBatchSink, AsyncPublishBatchWithCustomLimit) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto constexpr kMaxLinks = 5;
  auto constexpr kBatchSize = 6;
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_)).Times(kBatchSize);
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([batch_size = kBatchSize](
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(batch_size)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink =
      MakeTestBatchSink(std::move(mock), MakeTestOptions(kMaxLinks));

  auto span_catcher = InstallSpanCatcher();
  AddMessages(CreateSpans(kBatchSize), batch_sink);
  auto response = batch_sink->AsyncPublish(MakeRequest(kBatchSize)).get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanHasInstrumentationScope(), SpanKindIsClient(),
                         SpanNamed("test-topic publish"),
                         SpanHasAttributes(OTelAttribute<std::int64_t>(
                             sc::kMessagingBatchMessageCount, kBatchSize)))));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("publish #0"), SpanKindIsClient(),
                                    SpanLinksSizeIs(kMaxLinks))));
  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("publish #1"), SpanKindIsClient(),
                                    SpanLinksSizeIs(1))));
}

TEST(TracingBatchSink, AsyncPublishSpanAddsEvent) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  EXPECT_CALL(*mock, AddMessage(_));
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto message_spans = CreateSpans(1);
  AddMessages(message_spans, batch_sink, /*end_spans=*/false);
  auto response = batch_sink->AsyncPublish(MakeRequest(1)).get();
  EXPECT_THAT(response, IsOk());

  EndSpans(message_spans);

  EXPECT_THAT(span_catcher->GetSpans(),
              Contains(AllOf(SpanNamed("test span 0"),
                             SpanHasEvents(EventNamed("gl-cpp.publish_start"),
                                           EventNamed("gl-cpp.added_to_batch"),
                                           EventNamed("gl-cpp.publish_end")))));
}

TEST(TracingBatchSink, AsyncPublishAddsEventForMultipleMessages) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(2)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  EXPECT_CALL(*mock, AddMessage(_)).Times(2);
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto message_spans = CreateSpans(2);
  AddMessages(message_spans, batch_sink, /*end_spans=*/false);
  auto response = batch_sink->AsyncPublish(MakeRequest(2)).get();
  EXPECT_THAT(response, IsOk());

  EndSpans(message_spans);
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test span 0"),
                         SpanHasEvents(EventNamed("gl-cpp.publish_start")))));
  EXPECT_THAT(spans, Contains(AllOf(
                         SpanNamed("test span 1"),
                         SpanHasEvents(EventNamed("gl-cpp.publish_start")))));
}

TEST(TracingBatchSink, Scope) {
  namespace sc = ::opentelemetry::trace::SemanticConventions;
  auto span_catcher = InstallSpanCatcher();
  auto message_span = MakeSpan("test span");
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });

  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto initial_spans = {message_span};
  AddMessages(initial_spans, batch_sink);

  auto response = batch_sink->AsyncPublish(MakeRequest(1))
                      .then([](auto f) {
                        EXPECT_FALSE(ThereIsAnActiveSpan());
                        EXPECT_FALSE(OTelContextCaptured());
                        return f;
                      })
                      .get();
  EXPECT_THAT(response, IsOk());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanHasInstrumentationScope(), SpanKindIsClient(),
                             SpanNamed("test-topic publish"),
                             SpanHasAttributes(OTelAttribute<std::int64_t>(
                                 sc::kMessagingBatchMessageCount, 1)),
                             SpanHasLinks(AllOf(LinkHasSpanContext(
                                 message_span->GetContext()))))));
}

TEST(TracingBatchSink, ResumePublish) {
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, ResumePublish);
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  batch_sink->ResumePublish("unused");
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
TEST(TracingBatchSink, AsyncPublishAddsLink) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto message_spans = CreateSpans(1);
  AddMessages(message_spans, batch_sink, /*end_spans=*/false);
  auto response = batch_sink->AsyncPublish(MakeRequest(1)).get();
  EXPECT_THAT(response, IsOk());

  EndSpans(message_spans);

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(SpanNamed("test span 0"), SpanLinksSizeIs(1),
                     SpanHasEvents(EventNamed("gl-cpp.publish_start")))));
}
#else

TEST(TracingBatchSink, AsyncPublishAddsSpanIdAndTraceIdAttribute) {
  // The span catcher must be installed before the message span is created.
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AddMessage(_));
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_THAT(request, IsProtoEqual(MakeRequest(1)));
        return make_ready_future(make_status_or(MakeResponse(request)));
      });
  auto batch_sink = MakeTestBatchSink(std::move(mock));

  auto message_spans = CreateSpans(1);
  AddMessages(message_spans, batch_sink, /*end_spans=*/false);
  auto response = batch_sink->AsyncPublish(MakeRequest(1)).get();
  EXPECT_THAT(response, IsOk());

  EndSpans(message_spans);

  EXPECT_THAT(
      span_catcher->GetSpans(),
      Contains(AllOf(
          SpanNamed("test span 0"),
          SpanHasEvents(AllOf(
              EventNamed("gl-cpp.publish_start"),
              SpanEventAttributesAre(
                  OTelAttribute<std::string>("gcp_pubsub.publish.trace_id", _),
                  OTelAttribute<std::string>("gcp_pubsub.publish.span_id",
                                             _)))))));
}
#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
