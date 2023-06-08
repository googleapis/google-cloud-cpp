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
#include "google/cloud/internal/async_streaming_write_rpc_tracing.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;

template <typename Request, typename Response>
class MockAsyncStreamingWriteRpc
    : public AsyncStreamingWriteRpc<Request, Response> {
 public:
  ~MockAsyncStreamingWriteRpc() override = default;

  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<bool>, Write, (Request const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(future<StatusOr<Response>>, Finish, (), (override));
  MOCK_METHOD(StreamingRpcMetadata, GetRequestMetadata, (), (const, override));
};

using MockStream = MockAsyncStreamingWriteRpc<int, int>;
using TestedStream = AsyncStreamingWriteRpcTracing<int, int>;

TEST(AsyncStreamingWriteRpcTracing, Cancel) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).WillOnce([span] {
    // Verify that our "cancel" event is added before calling `TryCancel()` on
    // the underlying stream.
    span->AddEvent("test-only: underlying stream cancel");
  });
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(make_ready_future<StatusOr<int>>(
          internal::CancelledError("cancelled"))));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  stream.Cancel();
  (void)stream.Finish().get();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanEventsAre(EventNamed("cancel"),
                        EventNamed("test-only: underlying stream cancel")))));
}

TEST(AsyncStreamingWriteRpcTracing, Start) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(
          make_ready_future<StatusOr<int>>(internal::AbortedError("fail"))));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  EXPECT_TRUE(stream.Start().get());
  (void)stream.Finish().get();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(SpanNamed("span"),
                                       SpanHasAttributes(SpanAttribute<bool>(
                                           "gcloud.stream_started", true)))));
}

TEST(AsyncStreamingWriteRpcTracing, Write) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Write)
      .WillOnce(Return(make_ready_future(true)))
      .WillOnce(Return(make_ready_future(false)))
      .WillOnce(Return(make_ready_future(true)));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(
          make_ready_future<StatusOr<int>>(internal::AbortedError("fail"))));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  EXPECT_TRUE(stream.Write(100, grpc::WriteOptions{}).get());
  EXPECT_FALSE(stream.Write(200, grpc::WriteOptions{}).get());
  EXPECT_TRUE(stream.Write(300, grpc::WriteOptions{}.set_last_message()).get());
  (void)stream.Finish().get();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanEventsAre(
              AllOf(EventNamed("message"),
                    SpanEventAttributesAre(
                        SpanAttribute<std::string>("message.type", "SENT"),
                        SpanAttribute<int>("message.id", 1),
                        SpanAttribute<bool>("message.is_last", false),
                        SpanAttribute<bool>("message.success", true))),
              AllOf(EventNamed("message"),
                    SpanEventAttributesAre(
                        SpanAttribute<std::string>("message.type", "SENT"),
                        SpanAttribute<int>("message.id", 2),
                        SpanAttribute<bool>("message.is_last", false),
                        SpanAttribute<bool>("message.success", false))),
              AllOf(EventNamed("message"),
                    SpanEventAttributesAre(
                        SpanAttribute<std::string>("message.type", "SENT"),
                        SpanAttribute<int>("message.id", 3),
                        SpanAttribute<bool>("message.is_last", true),
                        SpanAttribute<bool>("message.success", true)))))));
}

TEST(AsyncStreamingWriteRpcTracing, WritesDone) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, WritesDone).WillOnce(Return(make_ready_future(false)));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(
          make_ready_future<StatusOr<int>>(internal::AbortedError("fail"))));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  EXPECT_FALSE(stream.WritesDone().get());
  (void)stream.Finish().get();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(AllOf(
                         SpanNamed("span"),
                         SpanEventsAre(EventNamed("gcloud.writes_done")))));
}

TEST(AsyncStreamingWriteRpcTracing, Finish) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(
          make_ready_future<StatusOr<int>>(internal::AbortedError("fail"))));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  EXPECT_THAT(stream.Finish().get(), StatusIs(StatusCode::kAborted, "fail"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanHasAttributes(SpanAttribute<std::string>("grpc.peer", _)),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"))));
}

TEST(AsyncStreamingWriteRpcTracing, GetRequestMetadata) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(StreamingRpcMetadata{{"key", "value"}}));

  auto span = MakeSpan("span");
  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  auto md = stream.GetRequestMetadata();
  EXPECT_THAT(md, ElementsAre(Pair("key", "value")));
}

TEST(AsyncStreamingWriteRpcTracing, SpanEndsOnDestruction) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  {
    auto mock = std::make_unique<MockStream>();
    auto span = MakeSpan("span");
    TestedStream stream(std::make_shared<grpc::ClientContext>(),
                        std::move(mock), span);

    auto spans = span_catcher->GetSpans();
    EXPECT_THAT(spans, IsEmpty());
  }

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanNamed("span")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
