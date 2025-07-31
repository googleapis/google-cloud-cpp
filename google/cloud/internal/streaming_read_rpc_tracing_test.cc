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
#include "google/cloud/internal/streaming_read_rpc_tracing.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SetServerMetadata;
using ::google::cloud::testing_util::SpanEventAttributesAre;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SetArgPointee;

template <typename ResponseType>
class MockStreamingReadRpc : public StreamingReadRpc<ResponseType> {
 public:
  ~MockStreamingReadRpc() override = default;
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(absl::optional<Status>, Read, (ResponseType*), (override));
  MOCK_METHOD(RpcMetadata, GetRequestMetadata, (), (const, override));
};

std::shared_ptr<grpc::ClientContext> context() {
  auto c = std::make_shared<grpc::ClientContext>();
  SetServerMetadata(*c, {});
  return c;
}

void VerifyStream(StreamingReadRpc<int>& stream,
                  std::vector<int> const& expected_values,
                  Status const& expected_status) {
  std::vector<int> values;
  int value;
  for (;;) {
    auto status = stream.Read(&value);
    if (status.has_value()) {
      EXPECT_EQ(status.value(), expected_status);
      break;
    }
    values.push_back(value);
  }
  EXPECT_EQ(values, expected_values);
}

TEST(StreamingReadRpcTracingTest, Cancel) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto mock = std::make_unique<MockStreamingReadRpc<int>>();
  EXPECT_CALL(*mock, Cancel).WillOnce([span] {
    // Verify that our "cancel" event is added before calling `TryCancel()` on
    // the underlying stream.
    span->AddEvent("test-only: underlying stream cancel");
  });
  EXPECT_CALL(*mock, Read).WillOnce(Return(Status()));

  StreamingReadRpcTracing<int> stream(context(), std::move(mock), span);
  stream.Cancel();
  VerifyStream(stream, {}, Status());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanEventsAre(EventNamed("gl-cpp.cancel"),
                        EventNamed("test-only: underlying stream cancel")))));
}

TEST(StreamingReadRpcTracingTest, Read) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_unique<MockStreamingReadRpc<int>>();
  EXPECT_CALL(*mock, Read)
      .WillOnce(DoAll(SetArgPointee<0>(100), Return(absl::nullopt)))
      .WillOnce(DoAll(SetArgPointee<0>(200), Return(absl::nullopt)))
      .WillOnce(DoAll(SetArgPointee<0>(300), Return(absl::nullopt)))
      .WillOnce(Return(Status()));

  auto span = MakeSpan("span");
  StreamingReadRpcTracing<int> stream(context(), std::move(mock), span);
  VerifyStream(stream, {100, 200, 300}, Status());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanHasAttributes(OTelAttribute<std::string>("grpc.peer", _)),
          SpanEventsAre(
              AllOf(EventNamed("message"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::string>("message.type", "RECEIVED"),
                        OTelAttribute<int>("message.id", 1))),
              AllOf(EventNamed("message"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::string>("message.type", "RECEIVED"),
                        OTelAttribute<int>("message.id", 2))),
              AllOf(EventNamed("message"),
                    SpanEventAttributesAre(
                        OTelAttribute<std::string>("message.type", "RECEIVED"),
                        OTelAttribute<int>("message.id", 3)))))));
}

TEST(StreamingReadRpcTracingTest, GetRequestMetadata) {
  auto mock = std::make_unique<MockStreamingReadRpc<int>>();
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"key", "value"}}, {{"tk", "v"}}}));

  auto span = MakeSpan("span");
  StreamingReadRpcTracing<int> stream(context(), std::move(mock), span);
  auto md = stream.GetRequestMetadata();
  EXPECT_THAT(md.headers, ElementsAre(Pair("key", "value")));
  EXPECT_THAT(md.trailers, ElementsAre(Pair("tk", "v")));
}

TEST(StreamingReadRpcTracingTest, SpanEndsOnDestruction) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  {
    auto mock = std::make_unique<MockStreamingReadRpc<int>>();
    auto span = MakeSpan("span");
    StreamingReadRpcTracing<int> stream(context(), std::move(mock), span);

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
