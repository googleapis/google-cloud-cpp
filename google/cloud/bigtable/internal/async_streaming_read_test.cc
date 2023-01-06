// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/async_streaming_read.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::MockFunction;
using ::testing::StrictMock;

struct FakeResponse {
  std::string value;
};

class MockAsyncStreamingReadRpc
    : public internal::AsyncStreamingReadRpc<FakeResponse> {
 public:
  ~MockAsyncStreamingReadRpc() override = default;

  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<FakeResponse>>, Read, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
  MOCK_METHOD(internal::StreamingRpcMetadata, GetRequestMetadata, (),
              (const, override));
};

TEST(AsyncStreamingReadTest, FullStream) {
  auto mock = absl::make_unique<StrictMock<MockAsyncStreamingReadRpc>>();
  StrictMock<MockFunction<future<bool>(FakeResponse)>> on_read;
  StrictMock<MockFunction<void(Status)>> on_finish;

  ::testing::InSequence s;
  EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::make_optional(FakeResponse{"v0"}));
  });
  EXPECT_CALL(on_read, Call).WillOnce([](FakeResponse const& r) {
    EXPECT_EQ(r.value, "v0");
    return make_ready_future(true);
  });
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::make_optional(FakeResponse{"v1"}));
  });
  EXPECT_CALL(on_read, Call).WillOnce([](FakeResponse const& r) {
    EXPECT_EQ(r.value, "v1");
    return make_ready_future(true);
  });
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::optional<FakeResponse>{});
  });
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(on_finish, Call(StatusIs(StatusCode::kOk)));

  PerformAsyncStreamingRead<FakeResponse>(
      std::move(mock), on_read.AsStdFunction(), on_finish.AsStdFunction());
}

TEST(AsyncStreamingReadTest, BadStart) {
  auto mock = absl::make_unique<StrictMock<MockAsyncStreamingReadRpc>>();
  StrictMock<MockFunction<future<bool>(FakeResponse)>> on_read;
  StrictMock<MockFunction<void(Status)>> on_finish;

  ::testing::InSequence s;
  EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(false); });
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status(StatusCode::kPermissionDenied, "fail"));
  });
  EXPECT_CALL(on_finish, Call(StatusIs(StatusCode::kPermissionDenied)));

  PerformAsyncStreamingRead<FakeResponse>(
      std::move(mock), on_read.AsStdFunction(), on_finish.AsStdFunction());
}

TEST(AsyncStreamingReadTest, CancelMidStream) {
  auto mock = absl::make_unique<StrictMock<MockAsyncStreamingReadRpc>>();
  StrictMock<MockFunction<future<bool>(FakeResponse)>> on_read;
  StrictMock<MockFunction<void(Status)>> on_finish;

  ::testing::InSequence s;
  EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::make_optional(FakeResponse{"v0"}));
  });
  EXPECT_CALL(on_read, Call).WillOnce([](FakeResponse const& r) {
    EXPECT_EQ(r.value, "v0");
    return make_ready_future(false);
  });
  EXPECT_CALL(*mock, Cancel);
  EXPECT_CALL(*mock, Read).Times(3).WillRepeatedly([] {
    return make_ready_future(absl::make_optional(FakeResponse{"ignored"}));
  });
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::optional<FakeResponse>{});
  });
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status(StatusCode::kCancelled, "cancelled"));
  });
  EXPECT_CALL(on_finish, Call(StatusIs(StatusCode::kCancelled)));

  PerformAsyncStreamingRead<FakeResponse>(
      std::move(mock), on_read.AsStdFunction(), on_finish.AsStdFunction());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
