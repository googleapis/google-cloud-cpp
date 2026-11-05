// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/open_object.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pair;

using MockStream = google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiReadObjectRequest,
    google::storage::v2::BidiReadObjectResponse>;

StatusOr<std::chrono::system_clock::time_point> CancelledTimer() {
  return internal::CancelledError("test-only", GCP_ERROR_INFO());
}

StatusOr<std::chrono::system_clock::time_point> MakeTimerStatus(
    future<bool> f) {
  if (!f.get()) return CancelledTimer();
  return make_status_or(std::chrono::system_clock::now());
}

TEST(OpenImpl, RequestParams) {
  auto constexpr kPlain = R"pb(
    read_object_spec {
      bucket: "projects/_/buckets/test-bucket-name"
      object: "test-object-unused"
      generation: 42
      read_handle { handle: "unused" }
    }
  )pb";
  auto constexpr kWithRoutingToken = R"pb(
    read_object_spec {
      bucket: "projects/_/buckets/test-bucket-name"
      object: "test-object-unused"
      generation: 42
      read_handle { handle: "unused" }
      routing_token: "test-routing-token"
    }
  )pb";

  auto params = [](auto text) {
    auto r = google::storage::v2::BidiReadObjectRequest{};
    (void)TextFormat::ParseFromString(text, &r);
    return RequestParams(r);
  };
  EXPECT_EQ(params(kPlain), "bucket=projects/_/buckets/test-bucket-name");
  EXPECT_EQ(params(kWithRoutingToken),
            "bucket=projects/_/buckets/test-bucket-name"
            "&routing_token=test-routing-token");
}

TEST(OpenImpl, Basic) {
  auto constexpr kText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    generation: 42
  )pb";
  auto constexpr kReadResponse = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-123" }
  )pb";

  auto request = google::storage::v2::BidiReadObjectRequest{};
  ASSERT_TRUE(
      TextFormat::ParseFromString(kText, request.mutable_read_object_spec()));
  auto expected_response = google::storage::v2::BidiReadObjectResponse{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadResponse, &expected_response));

  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject)
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const& context,
                    google::cloud::internal::ImmutableOptions const&) {
        google::cloud::testing_util::ValidateMetadataFixture md;
        auto metadata = md.GetMetadata(*context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-request-params",
                                  "bucket=projects/_/buckets/test-bucket")));
        auto stream = std::make_unique<MockStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
          return sequencer.PushBack("Start").then(
              [](auto f) { return f.get(); });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&sequencer, request](
                    google::storage::v2::BidiReadObjectRequest const& actual,
                    grpc::WriteOptions) {
                  EXPECT_THAT(actual, IsProtoEqual(request));
                  return sequencer.PushBack("Write").then(
                      [](auto f) { return f.get(); });
                });
        EXPECT_CALL(*stream, Read).WillOnce([&sequencer, expected_response]() {
          return sequencer.PushBack("Read").then([expected_response](auto) {
            return std::make_optional(expected_response);
          });
        });
        return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
      });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = pending.get();
  auto expected_result = [expected_response]() {
    return AllOf(Field(&OpenStreamResult::stream, NotNull()),
                 Field(&OpenStreamResult::first_response,
                       IsProtoEqual(expected_response)));
  };
  ASSERT_THAT(response, IsOkAndHolds(expected_result()));
}

TEST(OpenImpl, BasicReadHandle) {
  auto constexpr kText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    generation: 42
    read_handle { handle: "test-handle-1234" }
    routing_token: "test-token"
  )pb";
  auto constexpr kReadResponse = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-123" }
  )pb";

  auto request = google::storage::v2::BidiReadObjectRequest{};
  ASSERT_TRUE(
      TextFormat::ParseFromString(kText, request.mutable_read_object_spec()));
  auto expected_response = google::storage::v2::BidiReadObjectResponse{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadResponse, &expected_response));

  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject)
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const& context,
                    google::cloud::internal::ImmutableOptions const&) {
        google::cloud::testing_util::ValidateMetadataFixture md;
        auto metadata = md.GetMetadata(*context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-request-params",
                                  "bucket=projects/_/buckets/test-bucket"
                                  "&routing_token=test-token")));

        auto stream = std::make_unique<MockStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
          return sequencer.PushBack("Start").then(
              [](auto f) { return f.get(); });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&sequencer, expected_request = request](
                    google::storage::v2::BidiReadObjectRequest const& request,
                    grpc::WriteOptions) {
                  EXPECT_THAT(request, IsProtoEqual(expected_request));
                  return sequencer.PushBack("Write").then(
                      [](auto f) { return f.get(); });
                });
        EXPECT_CALL(*stream, Read).WillOnce([&sequencer, expected_response]() {
          return sequencer.PushBack("Read").then([expected_response](auto) {
            return std::make_optional(expected_response);
          });
        });
        return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
      });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = pending.get();
  auto expected_result = [expected_response]() {
    return AllOf(Field(&OpenStreamResult::stream, NotNull()),
                 Field(&OpenStreamResult::first_response,
                       IsProtoEqual(expected_response)));
  };
  ASSERT_THAT(response, IsOkAndHolds(expected_result()));
}

TEST(OpenImpl, StartError) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return PermanentError(); });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST(OpenImpl, WriteError) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return PermanentError(); });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST(OpenImpl, ReadError) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Read).WillOnce([&sequencer]() {
      return sequencer.PushBack("Read").then([](auto) {
        return std::optional<google::storage::v2::BidiReadObjectResponse>();
      });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return PermanentError(); });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST(OpenImpl, UnexpectedFinish) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal));
}

TEST(OpenImpl, TimeoutCancellation) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Read).WillOnce([&sequencer]() {
      return sequencer.PushBack("Read").then([](auto) {
        return std::optional<google::storage::v2::BidiReadObjectResponse>();
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(testing::AtLeast(1));
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then([](auto) {
        return Status(StatusCode::kCancelled, "Stream timeout");
      });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillRepeatedly([&sequencer](auto) {
    return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
  });

  CompletionQueue cq(mock_cq);
  Options options;
  options.set<storage::DownloadStallTimeoutOption>(std::chrono::seconds(1));
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions(std::move(options)),
      google::storage::v2::BidiReadObjectRequest{});
  future<StatusOr<OpenStreamResult>> pending = coro->Call();

  auto timer1 = sequencer.PopFrontWithName();
  EXPECT_EQ(timer1.second, "MakeRelativeTimer");
  auto start = sequencer.PopFrontWithName();
  EXPECT_EQ(start.second, "Start");
  start.first.set_value(true);
  timer1.first.set_value(false);

  auto timer2 = sequencer.PopFrontWithName();
  EXPECT_EQ(timer2.second, "MakeRelativeTimer");
  auto write = sequencer.PopFrontWithName();
  EXPECT_EQ(write.second, "Write");
  write.first.set_value(true);
  timer2.first.set_value(false);

  auto timer3 = sequencer.PopFrontWithName();
  EXPECT_EQ(timer3.second, "MakeRelativeTimer");
  auto read = sequencer.PopFrontWithName();
  EXPECT_EQ(read.second, "Read");
  timer3.first.set_value(true);
  read.first.set_value(true);

  auto finish = sequencer.PopFrontWithName();
  EXPECT_EQ(finish.second, "Finish");
  finish.first.set_value(true);

  StatusOr<OpenStreamResult> response = pending.get();
  EXPECT_THAT(response, StatusIs(StatusCode::kCancelled));
}

enum class CancelPhase { kStart, kWrite, kRead, kFinish };

class OpenObjectCancelPhaseTest : public ::testing::TestWithParam<CancelPhase> {
};

TEST_P(OpenObjectCancelPhaseTest, CancelsCleanlyAtHandshakePhase) {
  CancelPhase const phase = GetParam();
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer, phase] {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer] {
      return sequencer.PushBack("Start").then(
          [](future<bool> f) { return f.get(); });
    });
    if (phase == CancelPhase::kWrite || phase == CancelPhase::kRead) {
      EXPECT_CALL(*stream, Write).WillOnce([&sequencer] {
        return sequencer.PushBack("Write").then(
            [](future<bool> f) { return f.get(); });
      });
    }
    if (phase == CancelPhase::kRead) {
      EXPECT_CALL(*stream, Read).WillOnce([&sequencer] {
        return sequencer.PushBack("Read").then([](future<bool>) {
          return std::make_optional(
              google::storage::v2::BidiReadObjectResponse{});
        });
      });
    }
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer] {
      return sequencer.PushBack("Finish").then(
          [](future<bool>) { return Status{}; });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  future<StatusOr<OpenStreamResult>> pending = coro->Call();

  auto const pop = [&sequencer](char const* expected) {
    auto [p, name] = sequencer.PopFrontWithName();
    EXPECT_EQ(name, expected);
    return std::move(p);
  };

  promise<bool> start = pop("Start");
  if (phase == CancelPhase::kStart) pending.cancel();
  start.set_value(phase != CancelPhase::kFinish);

  if (phase == CancelPhase::kWrite || phase == CancelPhase::kRead) {
    promise<bool> write = pop("Write");
    if (phase == CancelPhase::kWrite) pending.cancel();
    write.set_value(true);
  }
  if (phase == CancelPhase::kRead) {
    promise<bool> read = pop("Read");
    pending.cancel();
    read.set_value(true);
  }

  promise<bool> finish = pop("Finish");
  if (phase == CancelPhase::kFinish) pending.cancel();
  finish.set_value(true);

  EXPECT_THAT(pending.get(), StatusIs(StatusCode::kCancelled));
}

INSTANTIATE_TEST_SUITE_P(CancelPhases, OpenObjectCancelPhaseTest,
                         ::testing::Values(CancelPhase::kStart,
                                           CancelPhase::kWrite,
                                           CancelPhase::kRead,
                                           CancelPhase::kFinish));

TEST(OpenImpl, ConcurrentCancelAndReadCompletion) {
  for (int iter = 0; iter != 25; ++iter) {
    std::mutex gate_mu;
    std::condition_variable gate_cv;
    int ready = 0;
    bool release = false;
    std::atomic<int> cancel_calls{0};
    auto read_promise = std::make_shared<
        promise<std::optional<google::storage::v2::BidiReadObjectResponse>>>();
    future<OpenStream::ReadType> read_future = read_promise->get_future();

    MockStorageStub mock;
    EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&] {
      auto stream = std::make_unique<::testing::NiceMock<MockStream>>();
      ON_CALL(*stream, Start).WillByDefault([] {
        return make_ready_future(true);
      });
      ON_CALL(*stream, Write)
          .WillByDefault(
              [](google::storage::v2::BidiReadObjectRequest const&,
                 grpc::WriteOptions) { return make_ready_future(true); });
      EXPECT_CALL(*stream, Read)
          .WillOnce(
              [f = std::move(read_future)]() mutable { return std::move(f); });
      ON_CALL(*stream, Cancel).WillByDefault([&cancel_calls] {
        ++cancel_calls;
      });
      ON_CALL(*stream, Finish).WillByDefault([] {
        return make_ready_future(Status{});
      });
      return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
    });

    CompletionQueue cq;
    auto coro = std::make_shared<OpenObject>(
        mock, cq, std::make_shared<grpc::ClientContext>(),
        internal::MakeImmutableOptions({}),
        google::storage::v2::BidiReadObjectRequest{});
    future<StatusOr<OpenStreamResult>> pending = coro->Call();

    auto const wait_at_gate = [&] {
      std::unique_lock<std::mutex> lock(gate_mu);
      ++ready;
      gate_cv.notify_all();
      gate_cv.wait(lock, [&] { return release; });
    };
    std::thread reader([&] {
      wait_at_gate();
      read_promise->set_value(
          std::make_optional(google::storage::v2::BidiReadObjectResponse{}));
    });
    std::thread canceller([&] {
      wait_at_gate();
      pending.cancel();
    });

    {
      std::unique_lock<std::mutex> lock(gate_mu);
      gate_cv.wait(lock, [&] { return ready == 2; });
      release = true;
    }
    gate_cv.notify_all();
    reader.join();
    canceller.join();

    StatusOr<OpenStreamResult> result = pending.get();
    if (result.ok()) {
      EXPECT_EQ(cancel_calls.load(), 0);
      EXPECT_FALSE(result->stream->IsCancelled());
    } else {
      EXPECT_THAT(result, StatusIs(StatusCode::kCancelled));
      EXPECT_EQ(cancel_calls.load(), 1);
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
