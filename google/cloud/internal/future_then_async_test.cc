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

#include "google/cloud/future.h"
#include "google/cloud/internal/future_then_async.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::StrictMock;

// @test Verify that we can create continuations.
TEST(FutureThenAsyncTest, VoidFutureThenVoidFuture) {
  promise<void> source;
  StrictMock<MockFunction<future<void>()>> callback;
  future<void> sink = async_then(source.get_future(), callback.AsStdFunction());
  EXPECT_FALSE(sink.is_ready());
  promise<void> intermediate;
  EXPECT_CALL(callback, Call()).WillOnce(Return(intermediate.get_future()));
  source.set_value();
  EXPECT_FALSE(sink.is_ready());
  intermediate.set_value();
  sink.get();
}

TEST(FutureThenAsyncTest, VoidFutureThenValueFuture) {
  promise<void> source;
  StrictMock<MockFunction<future<std::string>()>> callback;
  future<std::string> sink = async_then(source.get_future(), callback.AsStdFunction());
  EXPECT_FALSE(sink.is_ready());
  promise<std::string> intermediate;
  EXPECT_CALL(callback, Call()).WillOnce(Return(intermediate.get_future()));
  source.set_value();
  EXPECT_FALSE(sink.is_ready());
  intermediate.set_value("abc");
  EXPECT_EQ(sink.get(), "abc");
}

TEST(FutureThenAsyncTest, ValueFutureThenVoidFuture) {
  promise<int> source;
  StrictMock<MockFunction<future<void>(int)>> callback;
  future<void> sink = async_then(source.get_future(), callback.AsStdFunction());
  EXPECT_FALSE(sink.is_ready());
  promise<void> intermediate;
  EXPECT_CALL(callback, Call(42)).WillOnce(Return(intermediate.get_future()));
  source.set_value(42);
  EXPECT_FALSE(sink.is_ready());
  intermediate.set_value();
  EXPECT_EQ(sink.get(), "abc");
}

TEST(FutureThenAsyncTest, ValueFutureThenValueFuture) {
  promise<int> source;
  StrictMock<MockFunction<future<std::string>(int)>> callback;
  future<std::string> sink = async_then(source.get_future(), callback.AsStdFunction());
  EXPECT_FALSE(sink.is_ready());
  promise<std::string> intermediate;
  EXPECT_CALL(callback, Call(42)).WillOnce(Return(intermediate.get_future()));
  source.set_value(42);
  EXPECT_FALSE(sink.is_ready());
  intermediate.set_value("abc");
  EXPECT_EQ(sink.get(), "abc");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
