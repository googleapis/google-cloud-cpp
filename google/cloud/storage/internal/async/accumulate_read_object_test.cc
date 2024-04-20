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

#include "google/cloud/storage/internal/async/accumulate_read_object.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <numeric>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::RpcMetadata;
using ::google::cloud::storage::testing::MockAsyncObjectMediaStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::google::storage::v2::ReadObjectRequest;
using ::google::storage::v2::ReadObjectResponse;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::Unused;

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

std::unique_ptr<
    google::cloud::internal::AsyncStreamingReadRpc<ReadObjectResponse>>
MakeMockStreamPartial(int id, ReadObjectResponse response,
                      StatusCode code = StatusCode::kOk) {
  auto stream = std::make_unique<MockAsyncObjectMediaStream>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(std::move(response))))))
      .WillOnce(Return(ByMove(MakeClosingRead())));
  EXPECT_CALL(*stream, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status(code, "")))));
  EXPECT_CALL(*stream, GetRequestMetadata)
      .WillOnce(
          Return(RpcMetadata{{{"key", "value-" + std::to_string(id)}}, {}}));
  return stream;
}

TEST(AsyncAccumulateReadObjectTest, PartialSimple) {
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

  auto mock = std::make_unique<MockAsyncObjectMediaStream>();
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
      .WillOnce(Return(RpcMetadata{{{"key", "value"}}, {{"tk", "v"}}}));

  CompletionQueue cq;
  auto runner = std::thread{[](CompletionQueue cq) { cq.Run(); }, cq};
  auto response = AsyncAccumulateReadObjectPartial(cq, std::move(mock),
                                                   std::chrono::minutes(1))
                      .get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(response.payload,
              ElementsAre(IsProtoEqual(r0), IsProtoEqual(r1)));
  EXPECT_THAT(response.metadata.headers,
              UnorderedElementsAre(Pair("key", "value")));
  EXPECT_THAT(response.metadata.trailers,
              UnorderedElementsAre(Pair("tk", "v")));
  cq.Shutdown();
  runner.join();
}

TEST(AsyncAccumulateReadObjectTest, PartialStartTimeout) {
  AsyncSequencer<bool> async;
  auto cq = MakeMockedCompletionQueue(async);

  auto mock = std::make_unique<MockAsyncObjectMediaStream>();

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

  auto pending = AsyncAccumulateReadObjectPartial(
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
  EXPECT_THAT(response.metadata.headers, IsEmpty());
  EXPECT_THAT(response.metadata.trailers, IsEmpty());
}

TEST(AsyncAccumulateReadObjectTest, PartialReadTimeout) {
  AsyncSequencer<bool> async;
  auto cq = MakeMockedCompletionQueue(async);

  auto mock = std::make_unique<MockAsyncObjectMediaStream>();

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

  auto pending = AsyncAccumulateReadObjectPartial(
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
  EXPECT_THAT(response.metadata.headers, IsEmpty());
  EXPECT_THAT(response.metadata.trailers, IsEmpty());
}

TEST(AsyncAccumulateReadObjectTest, PartialFinishTimeout) {
  AsyncSequencer<bool> async;
  auto cq = MakeMockedCompletionQueue(async);

  auto mock = std::make_unique<MockAsyncObjectMediaStream>();

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
      .WillOnce(
          Return(RpcMetadata{{{"k0", "v0"}, {"k1", "v1"}}, {{"tk", "tv"}}}));

  int cancel_count = 0;
  EXPECT_CALL(*mock, Cancel).WillOnce([&] { ++cancel_count; });

  auto pending = AsyncAccumulateReadObjectPartial(
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
  EXPECT_THAT(response.metadata.headers,
              UnorderedElementsAre(Pair("k0", "v0"), Pair("k1", "v1")));
  EXPECT_THAT(response.metadata.trailers,
              UnorderedElementsAre(Pair("tk", "tv")));
}

TEST(AsyncAccumulateReadObjectTest, FullSimple) {
  auto constexpr kText0 = R"pb(
    checksummed_data {
      content: "message0: the quick brown fox jumps over the lazy dog"
      crc32c: 1234
    }
    object_checksums { crc32c: 2345 md5_hash: "test-only-invalid" }
    content_range { start: 1024 end: 2048 complete_length: 8192 }
    metadata {
      bucket: "projects/_/buckets/bucket-name"
      name: "object-name"
      generation: 123456
    }
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

  auto const r0_size = GetContent(r0.checksummed_data()).size();
  auto constexpr kReadOffset = 1024;
  auto constexpr kReadLimit = 2048;

  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce([&](Unused, Unused, Unused, ReadObjectRequest const& request) {
        EXPECT_EQ(request.read_offset(), kReadOffset);
        EXPECT_EQ(request.read_limit(), kReadLimit);
        return MakeMockStreamPartial(0, r0, StatusCode::kUnavailable);
      })
      .WillOnce([&](Unused, Unused, Unused, ReadObjectRequest const& request) {
        EXPECT_EQ(request.read_offset(), kReadOffset + r0_size);
        EXPECT_EQ(request.read_limit(), kReadLimit - r0_size);
        EXPECT_EQ(request.generation(), 123456);
        return MakeMockStreamPartial(1, r1);
      });

  MockFunction<std::shared_ptr<grpc::ClientContext>()> context_factory;
  EXPECT_CALL(context_factory, Call).Times(2).WillRepeatedly([] {
    return std::make_shared<grpc::ClientContext>();
  });

  CompletionQueue cq;
  auto runner = std::thread{[](CompletionQueue cq) { cq.Run(); }, cq};
  auto options = google::cloud::internal::MakeImmutableOptions(
      Options{}
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(3).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                                std::chrono::microseconds(4),
                                                2.0)
                  .clone())
          .set<storage::DownloadStallTimeoutOption>(std::chrono::minutes(1)));
  ReadObjectRequest request;
  request.set_read_offset(kReadOffset);
  request.set_read_limit(kReadLimit);
  auto response =
      AsyncAccumulateReadObjectFull(cq, mock, context_factory.AsStdFunction(),
                                    request, options)
          .get();
  EXPECT_STATUS_OK(response.status);
  EXPECT_THAT(response.payload,
              ElementsAre(IsProtoEqual(r0), IsProtoEqual(r1)));
  EXPECT_THAT(
      response.metadata.headers,
      UnorderedElementsAre(Pair("key", "value-0"), Pair("key", "value-1")));
  cq.Shutdown();
  runner.join();
}

TEST(AsyncAccumulateReadObjectTest, FullTooManyTransients) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).Times(4).WillRepeatedly([] {
    return MakeMockStreamPartial(0, ReadObjectResponse{},
                                 StatusCode::kUnavailable);
  });

  CompletionQueue cq;
  auto runner = std::thread{[](CompletionQueue cq) { cq.Run(); }, cq};
  auto options = google::cloud::internal::MakeImmutableOptions(
      Options{}
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(3).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                                std::chrono::microseconds(4),
                                                2.0)
                  .clone())
          .set<storage::DownloadStallTimeoutOption>(std::chrono::minutes(1)));
  auto response =
      AsyncAccumulateReadObjectFull(
          cq, mock, []() { return std::make_shared<grpc::ClientContext>(); },
          ReadObjectRequest{}, options)
          .get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kUnavailable));
  cq.Shutdown();
  runner.join();
}

TEST(AsyncAccumulateReadObjectTest, PermanentFailure) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce(Return(ByMove(MakeMockStreamPartial(
          0, ReadObjectResponse{}, StatusCode::kPermissionDenied))));

  CompletionQueue cq;
  auto runner = std::thread{[](CompletionQueue cq) { cq.Run(); }, cq};
  auto options = google::cloud::internal::MakeImmutableOptions(
      Options{}
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(3).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                                std::chrono::microseconds(4),
                                                2.0)
                  .clone())
          .set<storage::DownloadStallTimeoutOption>(std::chrono::minutes(1)));
  auto response =
      AsyncAccumulateReadObjectFull(
          cq, mock, []() { return std::make_shared<grpc::ClientContext>(); },
          ReadObjectRequest{}, options)
          .get();
  EXPECT_THAT(response.status, StatusIs(StatusCode::kPermissionDenied));
  cq.Shutdown();
  runner.join();
}

TEST(AsyncAccumulateReadObjectTest, ToResponse) {
  // To generate the CRC32C checksums use:
  //    /bin/echo -n $content > foo.txt && gsutil hash foo.txt
  // and then pipe the base64 encoded output, for example, the "How vexingly..."
  // text yields:
  //    echo 'StZ/gA==' | openssl base64 -d  | xxd
  //    Output: 00000000: 4ad6 7f80
  auto constexpr kText0 = R"pb(
    checksummed_data {
      content: "The quick brown fox jumps over the lazy dog"
      crc32c: 0x22620404
    }
    object_checksums { crc32c: 2345 md5_hash: "test-only-invalid" }
    content_range { start: 1024 end: 2048 complete_length: 8192 }
    metadata { bucket: "projects/_/buckets/bucket-name" name: "object-name" }
  )pb";
  auto constexpr kText1 = R"pb(
    checksummed_data {
      content: "How vexingly quick daft zebras jump!"
      crc32c: 0x4ad67f80
    }
    object_checksums { crc32c: 2345 md5_hash: "test-only-invalid" }
    content_range { start: 2048 end: 4096 complete_length: 8192 }
    metadata { bucket: "projects/_/buckets/bucket-name" name: "object-name" }
  )pb";

  ReadObjectResponse r0;
  ASSERT_TRUE(TextFormat::ParseFromString(kText0, &r0));
  ReadObjectResponse r1;
  ASSERT_TRUE(TextFormat::ParseFromString(kText1, &r1));

  AsyncAccumulateReadObjectResult accumulated;
  accumulated.status = Status{};
  accumulated.payload.push_back(r0);
  accumulated.payload.push_back(r1);
  accumulated.metadata.headers.emplace("key", "v0");
  accumulated.metadata.headers.emplace("key", "v1");
  accumulated.metadata.trailers.emplace("tk", "v0");
  accumulated.metadata.trailers.emplace("tk", "v1");

  auto const actual = ToResponse(accumulated);
  ASSERT_STATUS_OK(actual);
  auto const contents = actual->contents();
  auto const merged =
      std::accumulate(contents.begin(), contents.end(), std::string{},
                      [](auto a, auto b) { return a + std::string(b); });
  EXPECT_EQ(merged,
            "The quick brown fox jumps over the lazy dog"
            "How vexingly quick daft zebras jump!");
  EXPECT_THAT(actual->headers(),
              UnorderedElementsAre(Pair("key", "v0"), Pair("key", "v1"),
                                   Pair("tk", "v0"), Pair("tk", "v1")));
  EXPECT_THAT(
      actual->metadata(),
      Optional(AllOf(
          ResultOf(
              "metadata bucket value", [](auto const& m) { return m.bucket(); },
              "projects/_/buckets/bucket-name"),
          ResultOf(
              "metadata bucket value", [](auto const& m) { return m.name(); },
              "object-name"))));
  EXPECT_THAT(actual->offset(), 1024);
}

TEST(AsyncAccumulateReadObjectTest, ToResponseDataLoss) {
  auto constexpr kText0 = R"pb(
    checksummed_data {
      content: "The quick brown fox jumps over the lazy dog"
      crc32c: 0x00000000
    }
    object_checksums { crc32c: 2345 md5_hash: "test-only-invalid" }
    content_range { start: 1024 end: 2048 complete_length: 8192 }
    metadata { bucket: "projects/_/buckets/bucket-name" name: "object-name" }
  )pb";

  ReadObjectResponse r0;
  ASSERT_TRUE(TextFormat::ParseFromString(kText0, &r0));

  AsyncAccumulateReadObjectResult accumulated;
  accumulated.status = Status{};
  accumulated.payload.push_back(r0);
  accumulated.metadata.headers.emplace("key", "v0");
  accumulated.metadata.headers.emplace("key", "v1");
  accumulated.metadata.trailers.emplace("tk0", "v0");
  accumulated.metadata.trailers.emplace("tk1", "v1");

  auto const actual = ToResponse(accumulated);
  EXPECT_THAT(actual, StatusIs(StatusCode::kDataLoss));
}

TEST(AsyncAccumulateReadObjectTest, ToResponseError) {
  AsyncAccumulateReadObjectResult accumulated;
  accumulated.status = Status(StatusCode::kNotFound, "not found");
  accumulated.metadata.headers.emplace("key", "v0");
  accumulated.metadata.headers.emplace("key", "v1");
  accumulated.metadata.trailers.emplace("tk0", "v0");
  accumulated.metadata.trailers.emplace("tk1", "v1");

  auto const actual = ToResponse(accumulated);
  EXPECT_THAT(actual, StatusIs(StatusCode::kNotFound, "not found"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
