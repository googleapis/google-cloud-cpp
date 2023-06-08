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
#include "google/cloud/internal/streaming_write_rpc_tracing.h"
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
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;

template <typename RequestType, typename ResponseType>
class MockStreamingWriteRpc
    : public StreamingWriteRpc<RequestType, ResponseType> {
 public:
  ~MockStreamingWriteRpc() override = default;
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(bool, Write, (RequestType const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(StatusOr<ResponseType>, Close, (), (override));
  MOCK_METHOD(StreamingRpcMetadata, GetRequestMetadata, (), (const, override));
};

using MockStream = MockStreamingWriteRpc<int, int>;
using TestedStream = StreamingWriteRpcTracing<int, int>;

TEST(StreamingWriteRpcTracingTest, Cancel) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).WillOnce([span] {
    // Verify that our "cancel" event is added before calling `TryCancel()` on
    // the underlying stream.
    span->AddEvent("test-only: underlying stream cancel");
  });
  EXPECT_CALL(*mock, Close).WillOnce(Return(5));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  stream.Cancel();
  stream.Close();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(
          AllOf(SpanNamed("span"),
                SpanEventsAre(EventNamed("cancel"),
                              EventNamed("test-only: underlying stream cancel"),
                              EventNamed("close")))));
}

TEST(StreamingWriteRpcTracingTest, Write) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Write)
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock, Close).WillOnce(Return(5));

  auto span = MakeSpan("span");
  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  EXPECT_TRUE(stream.Write(100, grpc::WriteOptions{}));
  EXPECT_FALSE(stream.Write(200, grpc::WriteOptions{}));
  EXPECT_TRUE(stream.Write(300, grpc::WriteOptions{}.set_last_message()));
  stream.Close();

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
                        SpanAttribute<bool>("message.success", true))),
              EventNamed("close")))));
}

TEST(StreamingWriteRpcTracingTest, Close) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Close).WillOnce(Return(5));

  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  stream.Close();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(SpanNamed("span"),
                                SpanHasAttributes(SpanAttribute<std::string>(
                                    "grpc.peer", _)))));
}

TEST(StreamingWriteRpcTracingTest, GetRequestMetadata) {
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(StreamingRpcMetadata{{"key", "value"}}));

  auto span = MakeSpan("span");
  TestedStream stream(std::make_shared<grpc::ClientContext>(), std::move(mock),
                      span);
  auto md = stream.GetRequestMetadata();
  EXPECT_THAT(md, ElementsAre(Pair("key", "value")));
}

TEST(StreamingWriteRpcTracingTest, SpanEndsOnDestruction) {
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
