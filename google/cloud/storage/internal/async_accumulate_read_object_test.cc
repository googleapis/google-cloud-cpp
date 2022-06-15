// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/async_accumulate_read_object.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::StreamingRpcMetadata;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::google::storage::v2::ReadObjectResponse;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

class MockStream : public google::cloud::internal::AsyncStreamingReadRpc<
                       ReadObjectResponse> {
 public:
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<ReadObjectResponse>>, Read, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(StreamingRpcMetadata, GetRequestMetadata, (), (const, override));
};

future<absl::optional<ReadObjectResponse>> MakeClosingRead() {
  return make_ready_future(absl::optional<ReadObjectResponse>());
}

CompletionQueue MakeMockedCompletionQueue(AsyncSequencer<bool>& async) {
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, MakeRelativeTimer)
      .WillRepeatedly([&](std::chrono::nanoseconds duration) {
        using std::chrono::system_clock;
        auto const d =
            std::chrono::duration_cast<system_clock::duration>(duration);
        auto deadline = system_clock::now() + d;
        return async.PushBack("MakeRelativeTimer")
            .then([deadline](future<bool> f) {
              if (f.get()) return make_status_or(deadline);
              return StatusOr<std::chrono::system_clock::time_point>(
                  Status(StatusCode::kCancelled, "cancelled"));
            });
      });
  return CompletionQueue(std::move(mock));
}

TEST(AsyncAccumulateReadObjectTest, Simple) {
  auto constexpr kText0 = R"pb(
    checksummed_data {
      content: "message0: the quick brown fox jumps over the lazy dog"
      crc32c: 1234
    }
    object_checksums { crc32c: 2345 md5_hash: "test-only-invalid" }
    content_range { start: 1024 end: 2048 complete_length: 8192 }
    metadata { bucket: "projects/_/buckets/bucket-name" name: "object-name" }
  )pb";
  auto constexpr kText1 = R"pb(
    checksummed_data {
      content: "message1: the quick brown fox jumps over the lazy dog"
      crc32c: 1235
    }
  )pb";

  ReadObjectResponse r0;
  ASSERT_TRUE(TextFormat::ParseFromString(kText0, &r0));
  ReadObjectResponse r1;
  ASSERT_TRUE(TextFormat::ParseFromString(kText1, &r1));

  auto mock = absl::make_unique<MockStream>();
  ::testing::InSequence sequence;

  EXPECT_CALL(*mock, Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*mock, Read)
      .WillOnce(Return(ByMove(make_ready_future(absl::make_optional(r0)))))
      .WillOnce(Return(ByMove(make_ready_future(absl::make_optional(r1)))))
      .WillOnce(Return(ByMove(MakeClosingRead())));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "interrupted")))));
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(StreamingRpcMetadata{{"key", "value"}}));

  CompletionQueue cq;
  auto runner = std::thread{[](CompletionQueue cq) { cq.Run(); }, cq};
  auto response = storage_internal::AsyncAccumulateReadObject::Start(
                      cq, std::move(mock), std::chrono::minutes(1))
                      .get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(response.payload,
              ElementsAre(IsProtoEqual(r0), IsProtoEqual(r1)));
  EXPECT_THAT(response.metadata, UnorderedElementsAre(Pair("key", "value")));
  cq.Shutdown();
  runner.join();
}

TEST(AsyncAccumulateReadObjectTest, StartTimeout) {
  AsyncSequencer<bool> async;
  auto cq = MakeMockedCompletionQueue(async);

  auto mock = absl::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillRepeatedly([&] {
    return async.PushBack("Start").then([](future<bool> f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Read).WillRepeatedly([&] {
    return async.PushBack("Read").then([](future<bool> f) {
      if (f.get()) return absl::make_optional(ReadObjectResponse{});
      return absl::optional<ReadObjectResponse>();
    });
  });
  EXPECT_CALL(*mock, Finish).WillRepeatedly([&] {
    return async.PushBack("Finish").then([](future<bool> f) {
      if (f.get()) return Status{};
      return Status(StatusCode::kUnavailable, "broken");
    });
  });

  int cancel_count = 0;
  EXPECT_CALL(*mock, Cancel).WillOnce([&] { ++cancel_count; });

  auto pending = storage_internal::AsyncAccumulateReadObject::Start(
      cq, std::move(mock), std::chrono::milliseconds(1000));
  // We expect that just starting the "coroutine" will set up a timeout and
  // invoke `Start()`. We will have the timeout complete successfully, which
  // indicates `Start()` took too long.
  auto p0 = async.PopFrontWithName();
  auto p1 = async.PopFrontWithName();
  EXPECT_EQ(p0.second, "MakeRelativeTimer");
  EXPECT_EQ(p1.second, "Start");
  p0.first.set_value(true);
  EXPECT_EQ(cancel_count, 1);
  p1.first.set_value(false);

  // That should make the coroutine call Finish() to close the stream.
  auto f = async.PopFrontWithName();
  EXPECT_EQ(f.second, "Finish");
  f.first.set_value(true);

  // Now the coroutine should have finished
  auto response = pending.get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kDeadlineExceeded));
  EXPECT_THAT(response.payload, IsEmpty());
  EXPECT_THAT(response.metadata, IsEmpty());
}

TEST(AsyncAccumulateReadObjectTest, ReadTimeout) {
  AsyncSequencer<bool> async;
  auto cq = MakeMockedCompletionQueue(async);

  auto mock = absl::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillRepeatedly([&] {
    return async.PushBack("Start").then([](future<bool> f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Read).WillRepeatedly([&] {
    return async.PushBack("Read").then([](future<bool> f) {
      if (f.get()) return absl::make_optional(ReadObjectResponse{});
      return absl::optional<ReadObjectResponse>();
    });
  });
  EXPECT_CALL(*mock, Finish).WillRepeatedly([&] {
    return async.PushBack("Finish").then([](future<bool> f) {
      if (f.get()) return Status{};
      return Status(StatusCode::kUnavailable, "broken");
    });
  });

  int cancel_count = 0;
  EXPECT_CALL(*mock, Cancel).WillOnce([&] { ++cancel_count; });

  auto pending = storage_internal::AsyncAccumulateReadObject::Start(
      cq, std::move(mock), std::chrono::milliseconds(1000));
  // We expect that just starting the "coroutine" will set up a timeout and
  // invoke `Start()`. We will have the `Start()` complete successfully, and the
  // timeout canceled.
  auto p0 = async.PopFrontWithName();
  auto p1 = async.PopFrontWithName();
  EXPECT_EQ(p0.second, "MakeRelativeTimer");
  EXPECT_EQ(p1.second, "Start");
  p0.first.set_value(false);
  EXPECT_EQ(cancel_count, 0);
  p1.first.set_value(true);

  // This should trigger a new timer for the `Read()` call. This time we will
  // have the timer expire before `Read()` completes successfully.
  p0 = async.PopFrontWithName();
  p1 = async.PopFrontWithName();
  EXPECT_EQ(p0.second, "MakeRelativeTimer");
  EXPECT_EQ(p1.second, "Read");
  p0.first.set_value(true);
  EXPECT_EQ(cancel_count, 1);
  p1.first.set_value(false);

  // That should make the coroutine call Finish() to close the stream.
  auto f = async.PopFrontWithName();
  EXPECT_EQ(f.second, "Finish");
  f.first.set_value(true);

  // Now the coroutine should have finished
  auto response = pending.get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kDeadlineExceeded));
  EXPECT_THAT(response.payload, IsEmpty());
  EXPECT_THAT(response.metadata, IsEmpty());
}

TEST(AsyncAccumulateReadObjectTest, FinishTimeout) {
  AsyncSequencer<bool> async;
  auto cq = MakeMockedCompletionQueue(async);

  auto mock = absl::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillRepeatedly([&] {
    return async.PushBack("Start").then([](future<bool> f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Read).WillRepeatedly([&] {
    return async.PushBack("Read").then([](future<bool> f) {
      if (f.get()) return absl::make_optional(ReadObjectResponse{});
      return absl::optional<ReadObjectResponse>();
    });
  });
  EXPECT_CALL(*mock, Finish).WillRepeatedly([&] {
    return async.PushBack("Finish").then([](future<bool> f) {
      if (f.get()) return Status{};
      return Status(StatusCode::kCancelled, "cancel");
    });
  });
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(StreamingRpcMetadata{{"k0", "v0"}, {"k1", "v1"}}));

  int cancel_count = 0;
  EXPECT_CALL(*mock, Cancel).WillOnce([&] { ++cancel_count; });

  auto pending = storage_internal::AsyncAccumulateReadObject::Start(
      cq, std::move(mock), std::chrono::milliseconds(1000));
  // We expect that just starting the "coroutine" will set up a timeout and
  // invoke `Start()`. We will have the `Start()` complete successfully, and the
  // timeout canceled.
  auto p0 = async.PopFrontWithName();
  auto p1 = async.PopFrontWithName();
  EXPECT_EQ(p0.second, "MakeRelativeTimer");
  EXPECT_EQ(p1.second, "Start");
  p0.first.set_value(false);
  EXPECT_EQ(cancel_count, 0);
  p1.first.set_value(true);

  // This should trigger a new timer for the `Read()` call. This time we will
  // have the `Read()` call succeed before the timer expires.
  p0 = async.PopFrontWithName();
  p1 = async.PopFrontWithName();
  EXPECT_EQ(p0.second, "MakeRelativeTimer");
  EXPECT_EQ(p1.second, "Read");
  p0.first.set_value(false);
  EXPECT_EQ(cancel_count, 0);
  // Have the `Read()` call terminate the loop.
  p1.first.set_value(false);

  // That should trigger a new timer for the `Finish()` call, and a call to
  // `Finish()`.
  p0 = async.PopFrontWithName();
  p1 = async.PopFrontWithName();
  EXPECT_EQ(p0.second, "MakeRelativeTimer");
  EXPECT_EQ(p1.second, "Finish");
  // We have the timer expires before `Finish()` is completed
  p0.first.set_value(true);
  EXPECT_EQ(cancel_count, 1);
  p1.first.set_value(false);

  // Now the coroutine should have finished, note that the error code is
  // whatever Finish() returns.
  auto response = pending.get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kCancelled, "cancel"));
  EXPECT_THAT(response.payload, IsEmpty());
  EXPECT_THAT(response.metadata,
              UnorderedElementsAre(Pair("k0", "v0"), Pair("k1", "v1")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
