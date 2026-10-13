// Copyright 2025 Google LLC
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
#include "google/cloud/storage/internal/async/writer_connection_resumed.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/async/retry_policy.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/storage/v2/storage.pb.h"
#include <gmock/gmock.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::VariantWith;

using MockFactory =
    ::testing::MockFunction<future<StatusOr<WriteObject::WriteResult>>(
        google::storage::v2::BidiWriteObjectRequest)>;

using MockStreamingRpc =
    ::testing::MockFunction<std::unique_ptr<WriteObject::StreamingRpc>()>;

using MockStream = ::google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiWriteObjectRequest,
    google::storage::v2::BidiWriteObjectResponse>;

std::variant<std::int64_t, google::storage::v2::Object> MakePersistedState(
    std::int64_t persisted_size) {
  return persisted_size;
}

storage::WritePayload TestPayload(std::size_t n) {
  return storage::WritePayload(std::string(n, 'A'));
}

/// Options with non-zero watermarks, so small writes are not always flushed.
Options WatermarkOptions() {
  return Options{}
      .set<storage::BufferedUploadLwmOption>(16 * 1024)
      .set<storage::BufferedUploadHwmOption>(32 * 1024);
}

auto TestObject() {
  auto object = google::storage::v2::Object{};
  object.set_bucket("projects/_/buckets/test-bucket");
  object.set_name("test-object");
  return object;
}

/// Configures `mock` so every `Write()` and `Flush()` is recorded in
/// `sequencer` (as `"Write"` / `"Flush"`) and counted in `*calls`. A
/// successful `Flush()` advances `*persisted` by the payload size.
void RecordWritesAndFlushes(MockAsyncWriterConnection& mock,
                            AsyncSequencer<bool>& sequencer,
                            std::shared_ptr<std::int64_t> const& persisted,
                            std::shared_ptr<int> const& calls) {
  EXPECT_CALL(mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(mock, PersistedState).WillRepeatedly([persisted] {
    return MakePersistedState(*persisted);
  });
  EXPECT_CALL(mock, Write)
      .WillRepeatedly([&sequencer, calls](storage::WritePayload const&) {
        ++*calls;
        return sequencer.PushBack("Write").then([](auto) { return Status{}; });
      });
  EXPECT_CALL(mock, Flush)
      .WillRepeatedly(
          [&sequencer, persisted, calls](storage::WritePayload const& p) {
            ++*calls;
            auto const size = static_cast<std::int64_t>(p.size());
            return sequencer.PushBack("Flush").then([persisted, size](auto f) {
              if (!f.get()) return TransientError();
              *persisted += size;
              return Status{};
            });
          });
}

/// Configures the stream used after `Resume()`. Each stream `Write()` is
/// recorded as `"StreamFlush"` or `"StreamWrite"` and counted in `*calls`. A
/// flush persists all bytes up to the end of its payload.
void RecordStreamWrites(MockStream& stream, AsyncSequencer<bool>& sequencer,
                        std::shared_ptr<std::int64_t> const& persisted,
                        std::shared_ptr<int> const& calls) {
  EXPECT_CALL(stream, Write)
      .WillRepeatedly([&sequencer, persisted, calls](
                          google::storage::v2::BidiWriteObjectRequest const& r,
                          grpc::WriteOptions) {
        ++*calls;
        bool const flush = r.flush();
        std::int64_t const end =
            r.write_offset() +
            static_cast<std::int64_t>(GetContent(r.checksummed_data()).size());
        return sequencer.PushBack(flush ? "StreamFlush" : "StreamWrite")
            .then([persisted, flush, end](auto) {
              if (flush) *persisted = end;
              return true;
            });
      });
  EXPECT_CALL(stream, Read).WillRepeatedly([persisted] {
    google::storage::v2::BidiWriteObjectResponse response;
    response.set_persisted_size(*persisted);
    return make_ready_future(std::make_optional(response));
  });
  EXPECT_CALL(stream, Finish).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(stream, Cancel).WillRepeatedly(Return());
}

std::shared_ptr<storage::testing::MockHashFunction> MakeMockHash() {
  auto hash = std::make_shared<storage::testing::MockHashFunction>();
  EXPECT_CALL(*hash, Update(An<std::int64_t>(), An<absl::Cord const&>(),
                            An<std::uint32_t>()))
      .WillRepeatedly(Return(Status()));
  return hash;
}

TEST(WriteConnectionResumed, FinalizeEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  EXPECT_CALL(*mock, UploadId).WillOnce(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillRepeatedly([&](auto) {
    return sequencer.PushBack("Finalize")
        .then([](auto f) -> StatusOr<google::storage::v2::Object> {
          if (!f.get()) return TransientError();
          return TestObject();
        });
  });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto finalize = connection->Finalize({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionResumed, FinalizedOnConstruction) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly(Return(TestObject()));
  EXPECT_CALL(*mock, Finalize(_)).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return StatusOr<google::storage::v2::Object>(TestObject());
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(
      connection->PersistedState(),
      VariantWith<google::storage::v2::Object>(IsProtoEqual(TestObject())));

  auto finalize = connection->Finalize({});

  EXPECT_FALSE(finalize.is_ready());
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);
  EXPECT_TRUE(finalize.is_ready());
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionResumed, Cancel) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then(
        [](auto) { return Status(StatusCode::kCancelled, "cancel"); });
  });
  EXPECT_CALL(*mock, Cancel).Times(1);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto write = connection->Write(TestPayload(64 * 1024));
  ASSERT_FALSE(write.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  connection->Cancel();
  next.first.set_value(true);

  ASSERT_TRUE(write.is_ready());
  auto status = write.get();
  EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
}

TEST(WriterConnectionResumed, FlushEmpty) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, Flush)
      .WillRepeatedly([&, mock_persisted_size](auto const& p) {
        EXPECT_TRUE(p.payload().empty());
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          // Persisted size remains 0 for empty payload
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto flush = connection->Flush({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  EXPECT_THAT(flush.get(), StatusIs(StatusCode::kOk));
}

TEST(WriteConnectionResumed, FlushNonEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto const payload = TestPayload(1024);

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&, mock_persisted_size, payload](auto const& p) {
        EXPECT_EQ(p.payload(), payload.payload());
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto f) {
          if (!f.get()) return TransientError();
          *mock_persisted_size = 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto write = connection->Write(payload);
  ASSERT_FALSE(write.is_ready());

  auto flush = connection->Flush({});
  ASSERT_FALSE(flush.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  EXPECT_TRUE(flush.is_ready());
  EXPECT_THAT(flush.get(), StatusIs(StatusCode::kOk));

  EXPECT_TRUE(write.is_ready());
  EXPECT_THAT(write.get(), StatusIs(StatusCode::kOk));
}

// Verifies that multiple queued Flush() operations track cumulative byte
// offsets and that each flush future is satisfied only when the server's
// persisted_size reaches or exceeds its target offset.
TEST(WriteConnectionResumed, InterleavedMultiFlush) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto const payload1 = TestPayload(1024);
  auto const payload2 = TestPayload(1024);

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&, mock_persisted_size, payload1](auto const& p) {
        EXPECT_EQ(p.payload(), payload1.payload());
        return sequencer.PushBack("Flush1").then([mock_persisted_size](auto f) {
          if (!f.get()) return TransientError();
          *mock_persisted_size = 1024;
          return Status{};
        });
      })
      .WillOnce([&, mock_persisted_size, payload2](auto const& p) {
        EXPECT_EQ(p.payload(), payload2.payload());
        return sequencer.PushBack("Flush2").then([mock_persisted_size](auto f) {
          if (!f.get()) return TransientError();
          *mock_persisted_size = 2048;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  // Issue first Flush() of 1024 bytes.
  auto f1 = connection->Flush(payload1);
  ASSERT_FALSE(f1.is_ready());

  // Issue second Flush() of 1024 bytes while the first is still in-flight.
  auto f2 = connection->Flush(payload2);
  ASSERT_FALSE(f2.is_ready());

  // Complete first flush on mock (persisted_size reaches 1024).
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(true);

  // f1 must be ready with OK, while f2 must remain pending because its target
  // offset (2048 bytes) has not yet been reached.
  ASSERT_TRUE(f1.is_ready());
  EXPECT_THAT(f1.get(), StatusIs(StatusCode::kOk));
  ASSERT_FALSE(f2.is_ready());

  // Complete second flush on mock (persisted_size reaches 2048).
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);

  // Now f2 must also be ready with OK.
  ASSERT_TRUE(f2.is_ready());
  EXPECT_THAT(f2.get(), StatusIs(StatusCode::kOk));
}

TEST(WriteConnectionResumed, ResumeUsesWriteObjectSpecFromInitialRequest) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  initial_request.mutable_write_object_spec()->mutable_resource()->set_bucket(
      "projects/_/buckets/test-bucket");
  initial_request.mutable_write_object_spec()->mutable_resource()->set_name(
      "test-object");

  google::storage::v2::BidiWriteObjectResponse first_response;
  first_response.mutable_write_handle()->set_handle("test-handle");
  first_response.mutable_resource()->set_generation(12345);

  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));

  // Expect Flush to be called because flush_ is true by default.
  // Make it fail to trigger Resume().
  EXPECT_CALL(*mock, Flush(_)).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto f) {
      if (f.get()) return google::cloud::Status{};  // Should not be true
      return TransientError();
    });
  });

  MockFactory mock_factory;
  google::storage::v2::BidiWriteObjectRequest captured_request;
  EXPECT_CALL(mock_factory, Call(_))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest request) {
        captured_request = std::move(request);
        return sequencer.PushBack("Factory").then([](auto) {
          return StatusOr<WriteObject::WriteResult>(
              internal::AbortedError("stop test", GCP_ERROR_INFO()));
        });
      });

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  // This will call FlushStep -> mock->Flush()
  auto write = connection->Write(TestPayload(1));
  ASSERT_FALSE(write.is_ready());

  // Trigger the Flush error in the mock
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);  // This makes the lambda return TransientError

  // The error in OnFlush triggers Resume(), which calls the mock_factory.
  // Allow the factory callback to proceed.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  // The write future should now be ready, containing the error from the
  // factory.
  EXPECT_THAT(write.get(), StatusIs(StatusCode::kAborted));

  EXPECT_FALSE(captured_request.has_write_object_spec());
  EXPECT_TRUE(captured_request.has_append_object_spec());
  EXPECT_TRUE(captured_request.append_object_spec().has_write_handle());
  EXPECT_EQ(captured_request.append_object_spec().write_handle().handle(),
            "test-handle");
  EXPECT_EQ(captured_request.append_object_spec().generation(), 12345);
  EXPECT_EQ(captured_request.append_object_spec().object(), "test-object");
  EXPECT_EQ(captured_request.append_object_spec().bucket(),
            "projects/_/buckets/test-bucket");
}

TEST(WriteConnectionResumed, ResumeUsesAppendObjectSpecFromInitialRequest) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  initial_request.mutable_append_object_spec()->set_bucket(
      "projects/_/buckets/test-bucket");
  initial_request.mutable_append_object_spec()->set_object("test-object");

  google::storage::v2::BidiWriteObjectResponse first_response;
  first_response.mutable_resource()->set_generation(12345);

  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));

  // Expect Flush to be called because flush_ is true by default.
  // Make it fail to trigger Resume().
  EXPECT_CALL(*mock, Flush(_)).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto f) {
      if (f.get()) return google::cloud::Status{};  // Should not be true
      return TransientError();
    });
  });

  MockFactory mock_factory;
  google::storage::v2::BidiWriteObjectRequest captured_request;
  EXPECT_CALL(mock_factory, Call(_))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest request) {
        captured_request = std::move(request);
        return sequencer.PushBack("Factory").then([](auto) {
          return StatusOr<WriteObject::WriteResult>(
              internal::AbortedError("stop test", GCP_ERROR_INFO()));
        });
      });

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  // This will call FlushStep -> mock->Flush()
  auto write = connection->Write(TestPayload(1));
  ASSERT_FALSE(write.is_ready());

  // Trigger the Flush error in the mock
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);  // This makes the lambda return TransientError

  // The error in OnFlush triggers Resume(), which calls the mock_factory.
  // Allow the factory callback to proceed.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  // The write future should now be ready, containing the error from the
  // factory.
  EXPECT_THAT(write.get(), StatusIs(StatusCode::kAborted));

  EXPECT_FALSE(captured_request.has_write_object_spec());
  EXPECT_TRUE(captured_request.has_append_object_spec());
  EXPECT_FALSE(captured_request.append_object_spec().has_write_handle());
  EXPECT_EQ(captured_request.append_object_spec().generation(), 12345);
  EXPECT_EQ(captured_request.append_object_spec().object(), "test-object");
  EXPECT_EQ(captured_request.append_object_spec().bucket(),
            "projects/_/buckets/test-bucket");
}

TEST(WriteConnectionResumed, NoConcurrentWritesWhenFlushAndWriteRace) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, Flush(_)).WillRepeatedly([&](auto) {
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });

  // Make Write detect concurrent invocations. If two writes run concurrently
  // the compare_exchange will fail and the test will fail.
  std::atomic<bool> in_write{false};
  EXPECT_CALL(*mock, Write(_)).WillRepeatedly([&](auto) {
    bool expected = false;
    EXPECT_TRUE(in_write.compare_exchange_strong(expected, true));
    // Simulate some work that allows a concurrent write to attempt to run.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    in_write.store(false);
    return make_ready_future(Status{});
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  // Use non-zero watermarks so the 1 KiB `Write()` below is not blocked as a
  // high-water mark waiter. With `Options{}` it would only complete once the
  // buffer drains, which this mock (persisted size fixed at 0) never reports.
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, WatermarkOptions());

  // Start a flush which will call impl->Flush() and block.
  auto flush_future = connection->Flush({});

  // Immediately perform a user Write after the flush started. This can race
  // with the OnFlush-driven write continuation.
  auto write_future = connection->Write(TestPayload(1024));

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  // Wait for both futures to complete with a timeout to avoid indefinite hang.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!write_future.is_ready() &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!flush_future.is_ready() &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  ASSERT_TRUE(write_future.is_ready());
  ASSERT_TRUE(flush_future.is_ready());
  EXPECT_THAT(write_future.get(), StatusIs(StatusCode::kOk));
  EXPECT_THAT(flush_future.get(), StatusIs(StatusCode::kOk));
}

TEST(WriteConnectionResumed, WriteHandleAssignmentAfterResume) {
  struct {
    bool use_write_object_spec;
    std::string bucket, object, handle;
    std::int64_t generation;
  } cases[] = {
      {false, "projects/_/buckets/test-bucket", "test-object",
       "expected-handle", 12345},
      {true, "bucket1", "object1", "handle1", 111},
      {false, "bucket2", "object2", "handle2", 222},
  };

  for (auto const& tc : cases) {
    AsyncSequencer<bool> sequencer;
    auto mock = std::make_unique<MockAsyncWriterConnection>();
    google::storage::v2::BidiWriteObjectRequest req;
    if (tc.use_write_object_spec) {
      req.mutable_write_object_spec()->mutable_resource()->set_bucket(
          tc.bucket);
      req.mutable_write_object_spec()->mutable_resource()->set_name(tc.object);
    } else {
      req.mutable_append_object_spec()->set_bucket(tc.bucket);
      req.mutable_append_object_spec()->set_object(tc.object);
    }
    google::storage::v2::BidiWriteObjectResponse resp;
    resp.mutable_write_handle()->set_handle(tc.handle);
    resp.mutable_resource()->set_generation(tc.generation);

    EXPECT_CALL(*mock, PersistedState)
        .WillRepeatedly(Return(MakePersistedState(0)));
    EXPECT_CALL(*mock, Flush(_)).WillOnce([&](auto) {
      return sequencer.PushBack("Flush").then([](auto f) {
        if (f.get()) return google::cloud::Status{};
        return TransientError();
      });
    });

    MockFactory mock_factory;
    google::storage::v2::BidiWriteObjectRequest captured;
    EXPECT_CALL(mock_factory, Call(_))
        .WillOnce([&](google::storage::v2::BidiWriteObjectRequest r) {
          captured = std::move(r);
          return sequencer.PushBack("Factory").then([](auto) {
            return StatusOr<WriteObject::WriteResult>(
                internal::AbortedError("stop test", GCP_ERROR_INFO()));
          });
        });

    auto conn = MakeWriterConnectionResumed(mock_factory.AsStdFunction(),
                                            std::move(mock), req, nullptr, resp,
                                            Options{});
    auto write = conn->Write(TestPayload(1));
    sequencer.PopFrontWithName().first.set_value(false);
    sequencer.PopFrontWithName().first.set_value(true);

    EXPECT_THAT(write.get(), StatusIs(StatusCode::kAborted));
    EXPECT_TRUE(captured.has_append_object_spec());
    EXPECT_EQ(captured.append_object_spec().bucket(), tc.bucket);
    EXPECT_EQ(captured.append_object_spec().object(), tc.object);
    EXPECT_EQ(captured.append_object_spec().generation(), tc.generation);
    EXPECT_TRUE(captured.append_object_spec().has_write_handle());
    EXPECT_EQ(captured.append_object_spec().write_handle().handle(), tc.handle);
  }
}

TEST(WriterConnectionResumed, OnQueryUpdatesWriteHandle) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto* mock_ptr = mock.get();

  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  google::storage::v2::BidiWriteObjectResponse first_response;
  first_response.mutable_write_handle()->set_handle("initial-handle");

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock_ptr, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });

  google::storage::v2::BidiWriteHandle new_handle;
  new_handle.set_handle("updated-handle");
  EXPECT_CALL(*mock_ptr, WriteHandle).WillRepeatedly(Return(new_handle));

  auto const expected_payload = std::string(1024, 'A');

  EXPECT_CALL(*mock_ptr, Flush(_))
      .WillOnce([&, mock_persisted_size](auto const& p) {
        EXPECT_EQ(p.size(), expected_payload.size());
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto f) {
          if (f.get()) {
            *mock_persisted_size = 1024;
            return Status{};
          }
          return TransientError();
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto current_handle = connection->WriteHandle();
  ASSERT_TRUE(current_handle.has_value());
  EXPECT_EQ(current_handle->handle(), "initial-handle");

  auto flush = connection->Flush(storage::WritePayload(expected_payload));

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  EXPECT_THAT(flush.get(), StatusIs(StatusCode::kOk));

  current_handle = connection->WriteHandle();
  ASSERT_TRUE(current_handle.has_value());
  EXPECT_EQ(current_handle->handle(), "updated-handle");
}

TEST(WriterConnectionResumed, ResetWriteOffsetOnResume) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto* mock_ptr = mock.get();

  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  google::storage::v2::BidiWriteObjectResponse first_response;
  first_response.mutable_write_handle()->set_handle("initial-handle");

  auto mock_hash =
      std::make_shared<google::cloud::storage::testing::MockHashFunction>();
  EXPECT_CALL(*mock_hash, Update(::testing::An<std::int64_t>(),
                                 ::testing::An<absl::Cord const&>(),
                                 ::testing::An<std::uint32_t>()))
      .WillRepeatedly(Return(Status()));

  EXPECT_CALL(*mock_ptr, PersistedState)
      .WillOnce(Return(MakePersistedState(0)))
      .WillOnce(Return(MakePersistedState(1024)));

  auto const payload = TestPayload(2048);

  EXPECT_CALL(*mock_ptr, Flush(_)).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto f) {
      if (f.get()) return Status{};
      return TransientError();
    });
  });

  MockFactory mock_factory;
  auto mock_stream =
      std::make_unique<google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
          google::storage::v2::BidiWriteObjectRequest,
          google::storage::v2::BidiWriteObjectResponse>>();
  auto* mock_stream_ptr = mock_stream.get();

  EXPECT_CALL(mock_factory, Call(_))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const&) {
        WriteObject::WriteResult result;
        result.stream = std::move(mock_stream);
        result.first_response.mutable_write_handle()->set_handle("new-handle");
        return sequencer.PushBack("Factory").then(
            [r = std::move(result)](auto) mutable {
              return StatusOr<WriteObject::WriteResult>(std::move(r));
            });
      });

  EXPECT_CALL(*mock_stream_ptr, Write(_, _))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions) {
        EXPECT_EQ(GetContent(request.checksummed_data()).size(), 1024);
        EXPECT_EQ(GetContent(request.checksummed_data()),
                  std::string(1024, 'A'));
        return sequencer.PushBack("StreamWrite").then([](auto) {
          return true;
        });
      });

  google::storage::v2::BidiWriteObjectResponse read_response1;
  read_response1.set_persisted_size(2048);
  EXPECT_CALL(*mock_stream_ptr, Read).WillOnce([&, read_response1]() {
    return sequencer.PushBack("StreamRead1").then([read_response1](auto) {
      return std::make_optional(read_response1);
    });
  });

  EXPECT_CALL(*mock_stream_ptr, Finish)
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock_stream_ptr, Cancel).WillRepeatedly(Return());

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, mock_hash,
      first_response, Options{});

  auto write = connection->Write(payload);

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamWrite");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamRead1");
  next.first.set_value(true);

  EXPECT_THAT(write.get(), StatusIs(StatusCode::kOk));
}

TEST(WriterConnectionResumed, ResumeUsesSizeFromFirstResponse) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto* mock_ptr = mock.get();

  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  google::storage::v2::BidiWriteObjectResponse first_response;
  first_response.mutable_write_handle()->set_handle("initial-handle");

  auto mock_hash =
      std::make_shared<google::cloud::storage::testing::MockHashFunction>();
  EXPECT_CALL(*mock_hash, Update(::testing::An<std::int64_t>(),
                                 ::testing::An<absl::Cord const&>(),
                                 ::testing::An<std::uint32_t>()))
      .WillRepeatedly(Return(Status()));

  EXPECT_CALL(*mock_ptr, PersistedState)
      .WillOnce(Return(MakePersistedState(0)));

  auto const payload = TestPayload(2048);

  EXPECT_CALL(*mock_ptr, Flush(_)).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto f) {
      if (f.get()) return Status{};
      return TransientError();
    });
  });

  MockFactory mock_factory;
  auto mock_stream =
      std::make_unique<google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
          google::storage::v2::BidiWriteObjectRequest,
          google::storage::v2::BidiWriteObjectResponse>>();
  auto* mock_stream_ptr = mock_stream.get();

  EXPECT_CALL(mock_factory, Call(_))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const&) {
        WriteObject::WriteResult result;
        result.stream = std::move(mock_stream);
        result.first_response.mutable_write_handle()->set_handle("new-handle");
        result.first_response.set_persisted_size(2048);
        return sequencer.PushBack("Factory").then(
            [r = std::move(result)](auto) mutable {
              return StatusOr<WriteObject::WriteResult>(std::move(r));
            });
      });

  EXPECT_CALL(*mock_stream_ptr, Write(_, _))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions) {
        EXPECT_EQ(request.write_offset(), 2048);
        return sequencer.PushBack("StateLookupWrite").then([](auto) {
          return true;
        });
      });

  google::storage::v2::BidiWriteObjectResponse read_response;
  read_response.set_persisted_size(2048);
  EXPECT_CALL(*mock_stream_ptr, Read).WillOnce([&, read_response]() {
    return sequencer.PushBack("StreamRead1").then([read_response](auto) {
      return std::make_optional(read_response);
    });
  });

  EXPECT_CALL(*mock_stream_ptr, Finish)
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock_stream_ptr, Cancel).WillRepeatedly(Return());

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, mock_hash,
      first_response, Options{});

  auto write = connection->Write(payload);

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StateLookupWrite");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamRead1");
  next.first.set_value(true);

  EXPECT_THAT(write.get(), StatusIs(StatusCode::kOk));
}

TEST(WriterConnectionResumed, ResumeUsesChecksumsFromFirstResponse) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto* mock_ptr = mock.get();

  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  google::storage::v2::BidiWriteObjectResponse first_response;
  first_response.mutable_write_handle()->set_handle("initial-handle");

  auto mock_hash =
      std::make_shared<google::cloud::storage::testing::MockHashFunction>();
  EXPECT_CALL(*mock_hash, Update(::testing::An<std::int64_t>(),
                                 ::testing::An<absl::Cord const&>(),
                                 ::testing::An<std::uint32_t>()))
      .WillRepeatedly(Return(Status()));

  EXPECT_CALL(*mock_ptr, PersistedState)
      .WillOnce(Return(MakePersistedState(0)));

  auto const payload = TestPayload(2048);

  EXPECT_CALL(*mock_ptr, Flush(_)).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto f) {
      if (f.get()) return Status{};
      return TransientError();
    });
  });

  MockFactory mock_factory;
  auto mock_stream =
      std::make_unique<google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
          google::storage::v2::BidiWriteObjectRequest,
          google::storage::v2::BidiWriteObjectResponse>>();
  auto* mock_stream_ptr = mock_stream.get();

  EXPECT_CALL(mock_factory, Call(_))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const&) {
        WriteObject::WriteResult result;
        result.stream = std::move(mock_stream);
        result.first_response.mutable_write_handle()->set_handle("new-handle");
        result.first_response.set_persisted_size(2048);
        // Set checksums in response!
        result.first_response.mutable_persisted_data_checksums()->set_crc32c(
            12345);
        return sequencer.PushBack("Factory").then(
            [r = std::move(result)](auto) mutable {
              return StatusOr<WriteObject::WriteResult>(std::move(r));
            });
      });

  EXPECT_CALL(*mock_stream_ptr, Write(_, _))
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions) {
        EXPECT_EQ(request.write_offset(), 2048);
        return sequencer.PushBack("StateLookupWrite").then([](auto) {
          return true;
        });
      });

  google::storage::v2::BidiWriteObjectResponse read_response;
  read_response.set_persisted_size(2048);
  EXPECT_CALL(*mock_stream_ptr, Read).WillOnce([&, read_response]() {
    return sequencer.PushBack("StreamRead1").then([read_response](auto) {
      return std::make_optional(read_response);
    });
  });

  EXPECT_CALL(*mock_stream_ptr, Finish)
      .WillOnce(Return(make_ready_future(Status{})));
  EXPECT_CALL(*mock_stream_ptr, Cancel).WillRepeatedly(Return());

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, mock_hash,
      first_response, Options{});

  auto write = connection->Write(payload);

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Factory");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StateLookupWrite");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamRead1");
  next.first.set_value(true);

  EXPECT_THAT(write.get(), StatusIs(StatusCode::kOk));
}

TEST(WriterConnectionResumed, CloseEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  EXPECT_CALL(*mock, UploadId).WillOnce(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillRepeatedly([&](auto) {
    return sequencer.PushBack("Close").then([](auto f) -> Status {
      if (!f.get()) return TransientError();
      return Status{};
    });
  });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto close = connection->Close({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);
  EXPECT_STATUS_OK(close.get());
}

TEST(WriterConnectionResumed, DuplicateCloseFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([](auto) {
    return make_ready_future(Status{});
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto close1 = connection->Close({});
  auto close2 = connection->Close({});

  EXPECT_STATUS_OK(close1.get());
  EXPECT_THAT(close2.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriterConnectionResumed, CloseWithPayload) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  // The payload is flushed first.
  EXPECT_CALL(*mock, Flush(expected_write_size(8 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          *mock_persisted_size = 8 * 1024;
          return Status{};
        });
      });
  // Then the stream is closed with empty payload.
  EXPECT_CALL(*mock, Close(expected_write_size(0))).WillOnce([&](auto) {
    return sequencer.PushBack("Close").then([](auto) { return Status{}; });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto close = connection->Close(TestPayload(8 * 1024));
  ASSERT_FALSE(close.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);

  EXPECT_STATUS_OK(close.get());
}

TEST(WriterConnectionResumed, CloseFailsAndResumeFails) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto resume_error = PermanentError();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([&](auto) {
    return sequencer.PushBack("Close").then(
        [](auto) { return TransientError(); });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto) {
    return sequencer.PushBack("Retry").then(
        [&](auto) { return StatusOr<WriteObject::WriteResult>(resume_error); });
  });

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto close = connection->Close({});
  ASSERT_FALSE(close.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Retry");
  next.first.set_value(true);

  EXPECT_THAT(close.get(), StatusIs(resume_error.code()));
}

TEST(WriterConnectionResumed, CloseFailsAndResumeSucceedsButNotClosed) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto close_error = TransientError();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([&](auto) {
    return sequencer.PushBack("Close").then(
        [close_error](auto) { return close_error; });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto) {
    return sequencer.PushBack("Resume").then([](auto) {
      google::storage::v2::BidiWriteObjectResponse response;
      response.set_persisted_size(0);
      return StatusOr<WriteObject::WriteResult>(
          WriteObject::WriteResult{nullptr, response});
    });
  });

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto close = connection->Close({});
  ASSERT_FALSE(close.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  EXPECT_THAT(close.get(), StatusIs(close_error.code()));
}

TEST(WriterConnectionResumed, CloseFailsAndResumeSucceedsAndFinalized) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto close_error = TransientError();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([&](auto) {
    return sequencer.PushBack("Close").then(
        [close_error](auto) { return close_error; });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto) {
    return sequencer.PushBack("Resume").then([](auto) {
      google::storage::v2::BidiWriteObjectResponse response;
      *response.mutable_resource() = TestObject();
      return StatusOr<WriteObject::WriteResult>(
          WriteObject::WriteResult{nullptr, response});
    });
  });

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto close = connection->Close({});
  ASSERT_FALSE(close.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  EXPECT_STATUS_OK(close.get());
}

TEST(WriterConnectionResumed, CloseAfterFinalizeFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([](auto) {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto finalize = connection->Finalize({});
  auto close = connection->Close({});

  EXPECT_STATUS_OK(finalize.get());
  EXPECT_THAT(close.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriterConnectionResumed, FinalizeAfterCloseFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([](auto) {
    return make_ready_future(Status{});
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto close = connection->Close({});
  auto finalize = connection->Finalize({});

  EXPECT_STATUS_OK(close.get());
  EXPECT_THAT(finalize.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriterConnectionResumed, DuplicateFinalizeFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};

  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([](auto) {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto finalize1 = connection->Finalize({});
  auto finalize2 = connection->Finalize({});

  EXPECT_STATUS_OK(finalize1.get());
  EXPECT_THAT(finalize2.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionResumed, PermanentErrorNoResume) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto failed_precondition = Status(StatusCode::kFailedPrecondition,
                                    "another writer became exclusive");

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then(
        [failed_precondition](auto) { return failed_precondition; });
  });

  MockFactory mock_factory;
  // factory_ should NOT be called because error is permanent
  // (FAILED_PRECONDITION).
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto write = connection->Write(TestPayload(10));
  ASSERT_FALSE(write.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, Eq("Flush"));
  next.first.set_value(true);

  EXPECT_THAT(write.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionResumed, CustomRetryPolicyOption) {
  struct CustomAsyncRetryPolicy : public storage::AsyncRetryPolicy {
    std::unique_ptr<storage::AsyncRetryPolicy> clone() const override {
      return std::make_unique<CustomAsyncRetryPolicy>();
    }
    bool OnFailure(Status const&) override { return false; }
    bool IsExhausted() const override { return false; }
    bool IsPermanentFailure(Status const& s) const override {
      return s.code() == StatusCode::kInvalidArgument;
    }
  };

  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto invalid_argument =
      Status(StatusCode::kInvalidArgument, "custom permanent error");

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then(
        [invalid_argument](auto) { return invalid_argument; });
  });

  MockFactory mock_factory;
  // factory_ should NOT be called because error is permanent per custom policy.
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto options = Options{}.set<storage::AsyncRetryPolicyOption>(
      std::make_shared<CustomAsyncRetryPolicy>());

  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, options);

  auto write = connection->Write(TestPayload(10));
  ASSERT_FALSE(write.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, Eq("Flush"));
  next.first.set_value(true);

  EXPECT_THAT(write.get(), StatusIs(StatusCode::kInvalidArgument));
}

/// Test case for an operation issued from inside a `Flush()` continuation.
struct ChainedOpCase {
  std::string name;
  std::function<future<Status>(storage::AsyncWriterConnection&)> start_op;
  std::string expected_call;
};

class WriterConnectionResumedFlushCallbackTest
    : public ::testing::TestWithParam<ChainedOpCase> {};

/// @test Verify that an operation chained from a `Flush()` continuation is
/// dispatched to the underlying connection before the continuation returns.
TEST_P(WriterConnectionResumedFlushCallbackTest,
       ChainedOpDispatchedBeforeCallbackReturns) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);
  EXPECT_CALL(*mock, Close).WillRepeatedly([&](auto const&) {
    ++*calls;
    return sequencer.PushBack("Close").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Finalize).WillRepeatedly([&](auto const&) {
    ++*calls;
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  // Use non-zero watermarks so a small `Write()` is dispatched as a `Write`
  // rather than a `Flush` (the default low-water mark of 0 always flushes).
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto flush = connection->Flush(TestPayload(1024));
  ASSERT_THAT(*calls, Eq(1));

  // Chain the next operation from `flush`'s continuation. Because `flush` is
  // already satisfied when this callback runs, the writer must already be idle
  // so the chained operation dispatches synchronously (`*calls == 2`) before
  // the callback returns.
  future<Status> chained_op;
  auto callback_done = flush.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained_op = GetParam().start_op(*connection);
    EXPECT_THAT(*calls, Eq(2));
  });

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, Eq("Flush"));
  next.first.set_value(true);
  ASSERT_TRUE(callback_done.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, Eq(GetParam().expected_call));
  next.first.set_value(true);
  EXPECT_STATUS_OK(chained_op.get());
}

INSTANTIATE_TEST_SUITE_P(
    WriteConnectionResumed, WriterConnectionResumedFlushCallbackTest,
    ::testing::Values(ChainedOpCase{"Flush",
                                    [](storage::AsyncWriterConnection& c) {
                                      return c.Flush(TestPayload(1024));
                                    },
                                    "Flush"},
                      ChainedOpCase{"EmptyFlush",
                                    [](storage::AsyncWriterConnection& c) {
                                      return c.Flush(storage::WritePayload{});
                                    },
                                    "Flush"},
                      ChainedOpCase{"Write",
                                    [](storage::AsyncWriterConnection& c) {
                                      return c.Write(TestPayload(1024));
                                    },
                                    "Write"},
                      ChainedOpCase{"Close",
                                    [](storage::AsyncWriterConnection& c) {
                                      return c.Close(storage::WritePayload{});
                                    },
                                    "Close"},
                      ChainedOpCase{"Finalize",
                                    [](storage::AsyncWriterConnection& c) {
                                      return c.Finalize(storage::WritePayload{})
                                          .then([](auto f) {
                                            return f.get().status();
                                          });
                                    },
                                    "Finalize"}),
    [](::testing::TestParamInfo<ChainedOpCase> const& info) {
      return info.param.name;
    });

/// @test Verify that a `Flush()` continuation handing the next `Flush()` to
/// another thread does not race with `OnQuery()` on `state_`.
TEST(WriteConnectionResumed, FlushCallbackCrossThreadFlush) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto mock_persisted_size = std::make_shared<std::int64_t>(0);

  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, Flush).WillRepeatedly([&](storage::WritePayload const& p) {
    auto const size = static_cast<std::int64_t>(p.size());
    return sequencer.PushBack("Flush").then(
        [mock_persisted_size, size](auto f) {
          if (!f.get()) return TransientError();
          *mock_persisted_size += size;
          return Status{};
        });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto f1 = connection->Flush(TestPayload(1024));
  auto next = sequencer.PopFrontWithName();
  ASSERT_EQ(next.second, "Flush");

  std::thread worker;
  future<Status> f2;
  auto callback_done = f1.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    promise<void> started;
    worker = std::thread([&] {
      started.set_value();
      f2 = connection->Flush(TestPayload(1024));
    });
    started.get_future().wait();
    // Give the worker time to enter the writer before this continuation
    // returns and `SetFlushed()` restarts the write loop. Joining here would
    // add a happens-before edge and hide any race from TSAN. The sleep only
    // makes the interleaving likely: the assertions below hold for any
    // interleaving, so a slow scheduler reduces race coverage but cannot
    // make this test fail.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  });
  next.first.set_value(true);
  // Completing the sequencer promise runs the whole chain inline, including
  // the `f1` continuation, so `worker` is assigned before `set_value()`
  // returns.
  ASSERT_TRUE(callback_done.is_ready());
  worker.join();

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  EXPECT_STATUS_OK(f2.get());
}

/// @test Verify that `Flush()` futures complete in the order they were issued,
/// even if the underlying connection completes the next flush inline.
TEST(WriteConnectionResumed, FlushFuturesCompleteInOrderWithInlineCompletion) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto mock_persisted_size = std::make_shared<std::int64_t>(0);

  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&](storage::WritePayload const& p) {
        auto const size = static_cast<std::int64_t>(p.size());
        return sequencer.PushBack("Flush1").then(
            [mock_persisted_size, size](auto) {
              *mock_persisted_size += size;
              return Status{};
            });
      })
      // Return an already-satisfied future so the second flush's continuation
      // runs inline when `FlushStep()` dispatches it.
      .WillOnce([mock_persisted_size](storage::WritePayload const& p) {
        *mock_persisted_size += static_cast<std::int64_t>(p.size());
        return make_ready_future(Status{});
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto first_response = google::storage::v2::BidiWriteObjectResponse{};
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock), initial_request, nullptr,
      first_response, Options{});

  auto f1 = connection->Flush(TestPayload(1024));
  auto f2 = connection->Flush(TestPayload(1024));

  std::vector<int> completion_order;
  auto done1 = f1.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    completion_order.push_back(1);
  });
  auto done2 = f2.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    completion_order.push_back(2);
  });

  // Complete the first flush; the write loop then dispatches the second flush,
  // which completes inline. `f1` must still complete before `f2`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(true);

  ASSERT_TRUE(done1.is_ready());
  ASSERT_TRUE(done2.is_ready());
  EXPECT_THAT(completion_order, ElementsAre(1, 2));
}

/// @test Verify that a small `Write()` issued while a second `Flush()` is
/// queued does not cancel that pending flush.
TEST(WriteConnectionResumed, SmallWriteDoesNotCancelQueuedFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto f1 = connection->Flush(TestPayload(1024));
  auto f2 = connection->Flush(TestPayload(1024));
  // Below the low-water mark: on its own this `Write()` would not need a flush.
  auto w = connection->Write(TestPayload(1024));
  ASSERT_TRUE(w.is_ready());
  EXPECT_STATUS_OK(w.get());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(f1.is_ready());
  EXPECT_STATUS_OK(f1.get());
  ASSERT_FALSE(f2.is_ready());

  // `f2` is still pending, so the remaining data must be flushed, not written.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(f2.is_ready());
  EXPECT_STATUS_OK(f2.get());
}

/// @test Verify that a small `Write()` chained from a `Flush()` continuation
/// does not cancel a second `Flush()` that is still pending.
TEST(WriteConnectionResumed, ChainedSmallWriteDoesNotCancelQueuedFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto f1 = connection->Flush(TestPayload(1024));
  auto f2 = connection->Flush(TestPayload(1024));
  future<Status> chained;
  auto callback_done = f1.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained = connection->Write(TestPayload(1024));
  });

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(callback_done.is_ready());
  ASSERT_FALSE(f2.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(f2.is_ready());
  EXPECT_STATUS_OK(f2.get());
  EXPECT_STATUS_OK(chained.get());
}

/// @test Verify that `Write()` calls blocked at the high-water mark stay
/// blocked, and the buffer keeps being flushed, until the buffer drops below
/// the low-water mark.
TEST(WriteConnectionResumed, HwmWaitersStayBlockedUntilBelowLwm) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto w1 = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w1.is_ready());
  auto w2 = connection->Write(TestPayload(20 * 1024));
  ASSERT_FALSE(w2.is_ready());

  // Persisting the first 32 KiB leaves 20 KiB buffered, which is still above
  // the 16 KiB low-water mark.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  EXPECT_FALSE(w1.is_ready());
  EXPECT_FALSE(w2.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(w1.is_ready());
  EXPECT_STATUS_OK(w1.get());
  ASSERT_TRUE(w2.is_ready());
  EXPECT_STATUS_OK(w2.get());
}

/// @test Verify that an operation chained from a `Write()` blocked at the
/// high-water mark is dispatched before the continuation returns.
TEST(WriteConnectionResumed, HwmWriteCallbackChainedOpDispatchedBeforeReturn) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  future<Status> chained;
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained = connection->Write(TestPayload(1024));
    EXPECT_EQ(*calls, 2);
  });

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(callback_done.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  EXPECT_STATUS_OK(chained.get());
}

/// @test Verify that an operation chained from a `Write()` blocked at the
/// high-water mark is dispatched before the continuation returns when the
/// buffer shrinks as part of a `Resume()`.
TEST(WriteConnectionResumed, HwmWriteCallbackChainedOpDispatchedAfterResume) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);

  // The resumed stream reports that all 32 KiB were persisted.
  auto stream_persisted = std::make_shared<std::int64_t>(32 * 1024);
  auto stream_calls = std::make_shared<int>(0);
  auto stream = std::make_unique<MockStream>();
  RecordStreamWrites(*stream, sequencer, stream_persisted, stream_calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto const&) {
    WriteObject::WriteResult result;
    result.stream = std::move(stream);
    result.first_response.set_persisted_size(32 * 1024);
    return make_ready_future(
        StatusOr<WriteObject::WriteResult>(std::move(result)));
  });
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, MakeMockHash(),
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  future<Status> chained;
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained = connection->Write(TestPayload(1024));
    EXPECT_EQ(*stream_calls, 1);
  });

  // Fail the flush to trigger a `Resume()`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);
  ASSERT_TRUE(callback_done.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamWrite");
  next.first.set_value(true);
  EXPECT_STATUS_OK(chained.get());
}

/// @test Verify that a small `Write()` chained from a high-water mark
/// continuation during a `Resume()` does not cancel a pending `Flush()`.
TEST(WriteConnectionResumed, ChainedSmallWriteAfterResumeKeepsPendingFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);

  auto stream_persisted = std::make_shared<std::int64_t>(32 * 1024);
  auto stream_calls = std::make_shared<int>(0);
  auto stream = std::make_unique<MockStream>();
  RecordStreamWrites(*stream, sequencer, stream_persisted, stream_calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto const&) {
    WriteObject::WriteResult result;
    result.stream = std::move(stream);
    result.first_response.set_persisted_size(32 * 1024);
    return make_ready_future(
        StatusOr<WriteObject::WriteResult>(std::move(result)));
  });
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, MakeMockHash(),
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  auto flush = connection->Flush(TestPayload(1024));
  future<Status> chained;
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained = connection->Write(TestPayload(1024));
  });

  // Fail the flush. The resumed stream reports 32 KiB persisted, leaving
  // 1 KiB (below the low-water mark) buffered for the pending `Flush()`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);
  ASSERT_TRUE(callback_done.is_ready());
  ASSERT_FALSE(flush.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamFlush");
  next.first.set_value(true);
  ASSERT_TRUE(flush.is_ready());
  EXPECT_STATUS_OK(flush.get());
  EXPECT_STATUS_OK(chained.get());
}

/// @test Verify that a `Resume()` does not release `Write()` calls blocked at
/// the high-water mark while the buffer is still above the low-water mark.
TEST(WriteConnectionResumed, HwmWaitersStayBlockedAfterResumeUntilBelowLwm) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);

  // The resumed stream reports that nothing was persisted.
  auto stream_persisted = std::make_shared<std::int64_t>(0);
  auto stream_calls = std::make_shared<int>(0);
  auto stream = std::make_unique<MockStream>();
  RecordStreamWrites(*stream, sequencer, stream_persisted, stream_calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto const&) {
    WriteObject::WriteResult result;
    result.stream = std::move(stream);
    result.first_response.set_persisted_size(0);
    return make_ready_future(
        StatusOr<WriteObject::WriteResult>(std::move(result)));
  });
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, MakeMockHash(),
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());

  // Fail the flush to trigger a `Resume()`. All 32 KiB are still buffered,
  // which is above the 16 KiB low-water mark, so `w` must stay blocked.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(false);
  EXPECT_FALSE(w.is_ready());

  // Once the resumed stream persists the data, `w` is released.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StreamFlush");
  next.first.set_value(true);
  ASSERT_TRUE(w.is_ready());
  EXPECT_STATUS_OK(w.get());
}

/// @test Verify that, with a zero low-water mark, a `Write()` blocked at the
/// high-water mark is released once the buffer drains, and that the writer
/// does not keep issuing empty flushes afterwards.
TEST(WriteConnectionResumed, HwmWaiterReleasedWhenBufferDrainsWithZeroLwm) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{},
      Options{}
          .set<storage::BufferedUploadLwmOption>(0)
          .set<storage::BufferedUploadHwmOption>(1024));

  auto w = connection->Write(TestPayload(1024));
  ASSERT_FALSE(w.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  ASSERT_TRUE(w.is_ready());
  EXPECT_STATUS_OK(w.get());
  // The buffer is empty and no `Flush()` is pending, so no further calls.
  EXPECT_EQ(*calls, 1);
}

/// @test Verify that flush futures complete in FIFO order even when one server
/// flush satisfies several of them and a continuation chains a `Flush()` that
/// the underlying connection completes inline.
TEST(WriteConnectionResumed, ChainedInlineFlushDoesNotOvertakeEarlierFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([persisted] {
    return MakePersistedState(*persisted);
  });
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&sequencer, persisted](storage::WritePayload const& p) {
        auto const size = static_cast<std::int64_t>(p.size());
        return sequencer.PushBack("Flush1").then([persisted, size](auto) {
          *persisted += size;
          return Status{};
        });
      })
      // The chained `Flush()` completes inline, before `Flush()` returns.
      .WillOnce([persisted](storage::WritePayload const& p) {
        *persisted += static_cast<std::int64_t>(p.size());
        return make_ready_future(Status{});
      });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  std::vector<int> order;
  auto f1 = connection->Flush(TestPayload(1024));
  // Same target offset as `f1`, so one server flush satisfies both.
  auto f2 = connection->Flush(storage::WritePayload{});
  future<void> d3;
  auto d1 = f1.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    order.push_back(1);
    d3 = connection->Flush(TestPayload(1024)).then([&](future<Status> g) {
      EXPECT_STATUS_OK(g.get());
      order.push_back(3);
    });
  });
  auto d2 = f2.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    order.push_back(2);
  });

  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, Eq("Flush1"));
  next.first.set_value(true);
  ASSERT_TRUE(d1.is_ready());
  ASSERT_TRUE(d2.is_ready());
  ASSERT_TRUE(d3.is_ready());
  EXPECT_THAT(order, ElementsAre(1, 2, 3));
}

/// @test Verify that a `Flush()` chained from a `Write()` blocked at the
/// high-water mark, and completed inline, does not overtake a `Flush()` future
/// satisfied by the same server flush.
TEST(WriteConnectionResumed,
     HwmWaiterChainedInlineFlushDoesNotOvertakePendingFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([persisted] {
    return MakePersistedState(*persisted);
  });
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&sequencer, persisted](storage::WritePayload const& p) {
        auto const size = static_cast<std::int64_t>(p.size());
        return sequencer.PushBack("Flush1").then([persisted, size](auto) {
          *persisted += size;
          return Status{};
        });
      })
      .WillOnce([persisted](storage::WritePayload const& p) {
        *persisted += static_cast<std::int64_t>(p.size());
        return make_ready_future(Status{});
      });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  std::vector<int> order;
  // Released by the same server flush that satisfies `f2`.
  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  // Same target offset as the data already in flight.
  auto f2 = connection->Flush(storage::WritePayload{});
  future<void> d3;
  auto d1 = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    order.push_back(1);
    d3 = connection->Flush(TestPayload(1024)).then([&](future<Status> g) {
      EXPECT_STATUS_OK(g.get());
      order.push_back(3);
    });
  });
  auto d2 = f2.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    order.push_back(2);
  });

  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, Eq("Flush1"));
  next.first.set_value(true);
  ASSERT_TRUE(d1.is_ready());
  ASSERT_TRUE(d2.is_ready());
  ASSERT_TRUE(d3.is_ready());
  EXPECT_THAT(order, ElementsAre(1, 2, 3));
}

/// @test Verify that a chained operation failing inline with a permanent error
/// does not cause the write loop to issue another RPC on the failed
/// connection.
TEST(WriteConnectionResumed, ChainedOpPermanentErrorDoesNotRestartWriteLoop) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto write_calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([persisted] {
    return MakePersistedState(*persisted);
  });
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&sequencer, persisted](storage::WritePayload const& p) {
        auto const size = static_cast<std::int64_t>(p.size());
        return sequencer.PushBack("Flush").then([persisted, size](auto) {
          *persisted += size;
          return Status{};
        });
      });
  EXPECT_CALL(*mock, Write)
      .WillRepeatedly([write_calls](storage::WritePayload const&) {
        ++*write_calls;
        return make_ready_future(PermanentError());
      });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, nullptr,
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto f1 = connection->Flush(TestPayload(1024));
  auto callback_done = f1.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    // Fails inline; the data stays buffered on a now-failed connection.
    (void)connection->Write(TestPayload(1024));
  });

  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, Eq("Flush"));
  next.first.set_value(true);
  ASSERT_TRUE(callback_done.is_ready());
  // The buffered data must not be sent again on the failed connection.
  EXPECT_THAT(*write_calls, Eq(1));
  EXPECT_THAT(connection->Write(TestPayload(1)).get(),
              StatusIs(PermanentError().code()));
}

/// @test Verify that a chained operation failing inline with a permanent error
/// during a `Resume()` does not cause the write loop to issue another RPC on
/// the failed stream.
TEST(WriteConnectionResumed,
     ChainedOpPermanentErrorAfterResumeDoesNotRestartWriteLoop) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls);

  // Reporting all data persisted releases `w` as part of the resume itself;
  // the chained `Write()` then fails inline.
  auto stream_write_calls = std::make_shared<int>(0);
  auto stream = std::make_unique<MockStream>();
  EXPECT_CALL(*stream, Write)
      .WillRepeatedly([stream_write_calls](
                          google::storage::v2::BidiWriteObjectRequest const&,
                          grpc::WriteOptions) {
        ++*stream_write_calls;
        return make_ready_future(false);
      });
  EXPECT_CALL(*stream, Finish).WillRepeatedly([] {
    return make_ready_future(PermanentError());
  });
  EXPECT_CALL(*stream, Cancel).WillRepeatedly(Return());
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&](auto const&) {
    WriteObject::WriteResult result;
    result.stream = std::move(stream);
    result.first_response.set_persisted_size(32 * 1024);
    return make_ready_future(
        StatusOr<WriteObject::WriteResult>(std::move(result)));
  });
  auto connection = MakeWriterConnectionResumed(
      mock_factory.AsStdFunction(), std::move(mock),
      google::storage::v2::BidiWriteObjectRequest{}, MakeMockHash(),
      google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    (void)connection->Write(TestPayload(1024));
  });

  // A failed flush triggers the `Resume()`.
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, Eq("Flush"));
  next.first.set_value(false);
  ASSERT_TRUE(callback_done.is_ready());
  EXPECT_THAT(*stream_write_calls, Eq(1));
  EXPECT_THAT(connection->Write(TestPayload(1)).get(),
              StatusIs(PermanentError().code()));
}

/// Operations that may start the write loop from the calling thread.
enum class WriterOp { kWrite, kFlush, kFinalize, kClose };

std::string WriterOpName(WriterOp op) {
  switch (op) {
    case WriterOp::kWrite:
      return "Write";
    case WriterOp::kFlush:
      return "Flush";
    case WriterOp::kFinalize:
      return "Finalize";
    case WriterOp::kClose:
      return "Close";
  }
  return "Unknown";
}

/// Starts @p op on @p connection and returns its completion as a `Status`.
future<Status> StartWriterOp(storage::AsyncWriterConnection& connection,
                             WriterOp op) {
  switch (op) {
    case WriterOp::kWrite:
      return connection.Write(storage::WritePayload{});
    case WriterOp::kFlush:
      return connection.Flush(storage::WritePayload{});
    case WriterOp::kFinalize:
      return connection.Finalize(storage::WritePayload{})
          .then([](future<StatusOr<google::storage::v2::Object>> result) {
            return result.get().status();
          });
    case WriterOp::kClose:
      return connection.Close(storage::WritePayload{});
  }
  return make_ready_future(
      internal::UnknownError("unknown operation", GCP_ERROR_INFO()));
}

class WriterConnectionResumedLifetimeTest
    : public ::testing::TestWithParam<WriterOp> {};

/// @test Verify that no operation accesses the writer state after a
/// synchronous callback destroys the connection.
///
/// A second thread is required because the write loop is idle with a pending
/// future only while a `Flush()` callback is running, and that callback's
/// continuation holds a reference to the writer state. Calling the operation
/// from another thread while the `Flush()` callback is in flight lets the
/// callback unwind and drop its reference before the operation returns.
TEST_P(WriterConnectionResumedLifetimeTest,
       SurvivesConnectionDestroyedByCallback) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, WriteHandle).WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([persisted] {
    return MakePersistedState(*persisted);
  });
  EXPECT_CALL(*mock, Flush)
      .WillOnce([&sequencer, persisted](storage::WritePayload const& payload) {
        auto const size = static_cast<std::int64_t>(payload.size());
        return sequencer.PushBack("Flush").then(
            [persisted, size](future<bool>) {
              *persisted += size;
              return Status{};
            });
      })
      .WillOnce([](storage::WritePayload const&) {
        return make_ready_future(PermanentError());
      });
  EXPECT_CALL(*mock, Finalize).Times(0);
  EXPECT_CALL(*mock, Close).Times(0);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  std::unique_ptr<storage::AsyncWriterConnection> connection =
      MakeWriterConnectionResumed(
          mock_factory.AsStdFunction(), std::move(mock),
          google::storage::v2::BidiWriteObjectRequest{}, nullptr,
          google::storage::v2::BidiWriteObjectResponse{}, WatermarkOptions());

  // Refill the buffer to the high-water mark so `blocked_write` remains pending
  // after the first flush completes.
  future<Status> flush_future = connection->Flush(TestPayload(32 * 1024));
  future<Status> blocked_write = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(blocked_write.is_ready());

  promise<void> flush_callback_started;
  promise<void> unblock_flush_callback;
  promise<void> flush_callback_returned;
  future<void> flush_done = flush_future.then([&](future<Status> result) {
    EXPECT_STATUS_OK(result.get());
    flush_callback_started.set_value();
    unblock_flush_callback.get_future().wait();
  });

  // Destroy `connection` and wait for the main thread to unwind the first
  // flush's continuation so the terminal operation holds the last reference to
  // the writer state.
  Status write_status;
  future<void> write_done = blocked_write.then([&](future<Status> result) {
    write_status = result.get();
    connection.reset();
    unblock_flush_callback.set_value();
    flush_callback_returned.get_future().wait();
  });

  std::pair<promise<bool>, std::string> next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, Eq("Flush"));

  future<Status> op_future;
  std::thread op_thread([&] {
    flush_callback_started.get_future().wait();
    op_future = StartWriterOp(*connection, GetParam());
  });
  next.first.set_value(true);
  flush_callback_returned.set_value();
  op_thread.join();

  ASSERT_TRUE(flush_done.is_ready());
  ASSERT_TRUE(write_done.is_ready());
  EXPECT_THAT(write_status, StatusIs(PermanentError().code()));
  EXPECT_THAT(op_future.get(), StatusIs(PermanentError().code()));
}

INSTANTIATE_TEST_SUITE_P(WriteConnectionResumed,
                         WriterConnectionResumedLifetimeTest,
                         ::testing::Values(WriterOp::kWrite, WriterOp::kFlush,
                                           WriterOp::kFinalize,
                                           WriterOp::kClose),
                         [](::testing::TestParamInfo<WriterOp> const& info) {
                           return WriterOpName(info.param);
                         });

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
