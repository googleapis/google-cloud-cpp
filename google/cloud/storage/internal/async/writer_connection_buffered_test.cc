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

#include "google/cloud/storage/internal/async/writer_connection_buffered.h"
#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/async/retry_policy.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
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
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using MockFactory = ::testing::MockFunction<
    future<StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>>()>;

Options TestOptions() {
  return Options{}
      .set<storage::BufferedUploadLwmOption>(16 * 1024)
      .set<storage::BufferedUploadHwmOption>(32 * 1024);
}

std::variant<std::int64_t, google::storage::v2::Object> MakePersistedState(
    std::int64_t persisted_size) {
  return persisted_size;
}

auto TestObject() {
  auto object = google::storage::v2::Object{};
  object.set_bucket("projects/_/buckets/test-bucket");
  object.set_name("test-object");
  return object;
}

storage::WritePayload TestPayload(std::size_t n) {
  return storage::WritePayload(std::string(n, 'A'));
}

/// Configures `mock` so every `Write()` and `Flush()` is recorded in
/// `sequencer` (as `"Write" + tag` / `"Flush" + tag`) and counted in `*calls`.
/// A successful `Flush()` advances `*persisted` by the payload size.
void RecordWritesAndFlushes(MockAsyncWriterConnection& mock,
                            AsyncSequencer<bool>& sequencer,
                            std::shared_ptr<std::int64_t> const& persisted,
                            std::shared_ptr<int> const& calls,
                            std::string const& tag) {
  EXPECT_CALL(mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(mock, PersistedState).WillRepeatedly([persisted] {
    return MakePersistedState(*persisted);
  });
  EXPECT_CALL(mock, Write)
      .WillRepeatedly([&sequencer, calls, tag](storage::WritePayload const&) {
        ++*calls;
        return sequencer.PushBack("Write" + tag).then([](auto) {
          return Status{};
        });
      });
  EXPECT_CALL(mock, Flush)
      .WillRepeatedly(
          [&sequencer, persisted, calls, tag](storage::WritePayload const& p) {
            ++*calls;
            auto const size = static_cast<std::int64_t>(p.size());
            return sequencer.PushBack("Flush" + tag)
                .then([persisted, size](auto f) {
                  if (!f.get()) return TransientError();
                  *persisted += size;
                  return Status{};
                });
          });
}

TEST(WriteConnectionBuffered, FinalizeEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
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

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto finalize = connection->Finalize({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, FinalizedOnConstruction) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly(Return(TestObject()));
  EXPECT_CALL(*mock, Finalize).Times(0);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(
      connection->PersistedState(),
      VariantWith<google::storage::v2::Object>(IsProtoEqual(TestObject())));

  auto finalize = connection->Finalize({});
  EXPECT_TRUE(finalize.is_ready());
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, FinalizedOnResume) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  // This Finalize call will fail, triggering a resume.
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return StatusOr<google::storage::v2::Object>(TransientError());
    });
  });

  MockFactory mock_factory;
  // The resume will succeed and return a new mock that reports the object is
  // already finalized.
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then([](auto) {
      auto resumed_mock = std::make_unique<MockAsyncWriterConnection>();
      EXPECT_CALL(*resumed_mock, PersistedState)
          .WillRepeatedly(Return(TestObject()));
      return make_status_or(std::unique_ptr<storage::AsyncWriterConnection>(
          std::move(resumed_mock)));
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto finalize = connection->Finalize({});
  ASSERT_FALSE(finalize.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  // The overall operation should succeed because the resume found the finalized
  // object.
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, FinalizeFailsAndResumeFails) {
  AsyncSequencer<bool> sequencer;
  auto resume_error = PermanentError();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return StatusOr<google::storage::v2::Object>(TransientError());
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Retry").then([&](auto) {
      return StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
          resume_error);
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto finalize = connection->Finalize({});
  ASSERT_FALSE(finalize.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Retry");
  next.first.set_value(true);

  // The finalization should fail with the error from the failed resume attempt.
  EXPECT_THAT(finalize.get(), StatusIs(resume_error.code()));
}

TEST(WriteConnectionBuffered, WriteResumes) {
  AsyncSequencer<bool> sequencer;
  auto make_mock =
      [&sequencer]() -> std::unique_ptr<storage::AsyncWriterConnection> {
    auto mock = std::make_unique<MockAsyncWriterConnection>();
    EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
    EXPECT_CALL(*mock, PersistedState)
        .WillRepeatedly(Return(MakePersistedState(0)));
    EXPECT_CALL(*mock, Write).WillRepeatedly([&](auto) {
      return sequencer.PushBack("Write").then([](auto f) {
        if (!f.get()) return TransientError();
        return Status{};
      });
    });
    return mock;
  };

  auto mock = make_mock();
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return make_ready_future(make_status_or(make_mock()));
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto write = connection->Write(TestPayload(16));
  ASSERT_TRUE(write.is_ready());
  EXPECT_STATUS_OK(write.get());
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
}

TEST(WriteConnectionBuffered, WriteBuffers) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(24 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          *mock_persisted_size = 32 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  // TestOptions() configures the buffered writer to block once 32KiB of data
  // are buffered. First verify we can write without blocking.
  auto w0 = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(w0.is_ready());
  EXPECT_STATUS_OK(w0.get());

  // The first connection->Write() should trigger a Write() request to drain the
  // buffer.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");

  auto w1 = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(w1.is_ready());
  EXPECT_STATUS_OK(w1.get());
  auto w2 = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(w2.is_ready());
  EXPECT_STATUS_OK(w2.get());

  // This blocks as it fills the buffer.
  auto w3 = connection->Write(TestPayload(8 * 1024));
  ASSERT_FALSE(w3.is_ready());

  // Satisfying the first Write() call should trigger a second one.
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  ASSERT_TRUE(w3.is_ready());
  EXPECT_STATUS_OK(w3.get());
}

TEST(WriteConnectionBuffered, WritePartialFlushAndFinalize) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(24 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          *mock_persisted_size = 24 * 1024;
          return Status{};
        });
      });
  // The Finalize() call will flush the remaining data.
  EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush2").then([mock_persisted_size](auto) {
          *mock_persisted_size = 24 * 1024 + 32 * 1024;
          return Status{};
        });
      });
  EXPECT_CALL(*mock, Finalize(expected_write_size(0))).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  // TestOptions() configures the buffered writer to block once 32KiB of data
  // are buffered. First verify we can write without blocking.
  auto w0 = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(w0.is_ready());
  EXPECT_STATUS_OK(w0.get());

  // The first connection->Write() should trigger a Write() request to drain the
  // buffer.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");

  // Simulate a large write that fills the buffer
  auto w1 = connection->Write(TestPayload(24 * 1024));
  ASSERT_FALSE(w1.is_ready());

  // Satisfying the first Write() call should trigger a second one.
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  // That should free enough data in the buffer to continue writing.
  ASSERT_TRUE(w1.is_ready());
  EXPECT_STATUS_OK(w1.get());

  // Assume there is now a Finalize() call.
  auto finalize = connection->Finalize(TestPayload(32 * 1024));
  EXPECT_FALSE(finalize.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, ReconnectError) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, PersistedState)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Write).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then(
        [](auto) { return TransientError(); });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(1).WillOnce([&sequencer] {
    return sequencer.PushBack("Retry").then([](auto) {
      return StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
          TransientError());
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto write = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(write.is_ready());
  EXPECT_STATUS_OK(write.get());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);

  // At this point `mock_factory()` should have been called.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Retry");
  next.first.set_value(false);

  // Normally the factory implements the retry loop. In this test that is
  // simulated by `mock_factory`. Once the retry loop fails all RPCs fail.
  write = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(write.is_ready());
  EXPECT_THAT(write.get(), StatusIs(TransientError().code()));

  auto flush = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(flush.is_ready());
  EXPECT_THAT(flush.get(), StatusIs(TransientError().code()));

  auto finalize = connection->Finalize(TestPayload(8 * 1024));
  ASSERT_TRUE(finalize.is_ready());
  EXPECT_THAT(finalize.get(), StatusIs(TransientError().code()));
}

TEST(WriteConnectionBuffered, FinalizeSucceedsThenError) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });
  // This background Write will fail, triggering a resume.
  EXPECT_CALL(*mock, Write).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then(
        [](auto) { return TransientError(); });
  });

  MockFactory mock_factory;
  // The resume attempt will fail permanently.
  EXPECT_CALL(mock_factory, Call).WillOnce([&] {
    return sequencer.PushBack("Resume").then(
        [](auto) -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          return PermanentError();
        });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Successfully finalize.
  auto finalize = connection->Finalize({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));

  // The first Write() after finalize.
  auto write1 = connection->Write(TestPayload(1));
  EXPECT_STATUS_OK(write1.get());

  // Now, drive the background operations that lead to the permanent error
  // state.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  // After the background error, any new Write() calls should fail immediately.
  auto write2 = connection->Write(TestPayload(1));
  EXPECT_THAT(write2.get(), StatusIs(PermanentError().code()));
}

TEST(WriteConnectionBuffered, RewindError) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock_persisted_size = std::make_shared<std::int64_t>(32 * 1024);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          *mock_persisted_size = 16 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(),
              VariantWith<std::int64_t>(32 * 1024));

  // TestOptions() configures the buffered writer to block once 32KiB of data
  // are buffered. First verify we can write without blocking.
  auto write = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(write.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  ASSERT_TRUE(write.is_ready());
  auto status = write.get();
  EXPECT_THAT(status, StatusIs(StatusCode::kInternal));
  EXPECT_THAT(
      status.error_info().metadata(),
      IsSupersetOf({Pair("gcloud-cpp.storage.upload_id", "test-upload-id"),
                    Pair("gcloud-cpp.storage.resend_offset", "32768"),
                    Pair("gcloud-cpp.storage.persisted_size", "16384")}));
}

TEST(WriteConnectionBuffered, FastForwardError) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock_persisted_size = std::make_shared<std::int64_t>(32 * 1024);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          *mock_persisted_size = 128 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(),
              VariantWith<std::int64_t>(32 * 1024));

  // TestOptions() configures the buffered writer to block once 32KiB of data
  // are buffered. First verify we can write without blocking.
  auto write = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(write.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  ASSERT_TRUE(write.is_ready());
  auto status = write.get();
  EXPECT_THAT(status, StatusIs(StatusCode::kInternal));
  EXPECT_THAT(
      status.error_info().metadata(),
      IsSupersetOf({Pair("gcloud-cpp.storage.upload_id", "test-upload-id"),
                    Pair("gcloud-cpp.storage.resend_offset", "32768"),
                    Pair("gcloud-cpp.storage.persisted_size", "131072")}));
}

TEST(WriteConnectionBuffered, Cancel) {
  AsyncSequencer<bool> sequencer;

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

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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

TEST(WriteConnectionBuffered, Flush) {
  AsyncSequencer<bool> sequencer;

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
  EXPECT_CALL(*mock, Flush(expected_write_size(8 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          *mock_persisted_size = 8 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto w0 = connection->Flush(TestPayload(8 * 1024));
  ASSERT_FALSE(w0.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  EXPECT_STATUS_OK(w0.get());
}

TEST(WriteConnectionBuffered, FlushWithEmptyPayload) {
  AsyncSequencer<bool> sequencer;
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
  // This flush is for the empty payload.
  EXPECT_CALL(*mock, Flush(expected_write_size(0)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush").then([mock_persisted_size](auto) {
          // *mock_persisted_size = 0; // Empty payload, so it's 0, which is the
          // current value
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  auto f = connection->Flush({});
  ASSERT_FALSE(f.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  EXPECT_STATUS_OK(f.get());
}

TEST(WriteConnectionBuffered, ErrorFailsPendingFlushes) {
  AsyncSequencer<bool> sequencer;
  auto flush_error = TransientError();
  auto resume_error = Status(StatusCode::kAborted, "resume loop failed");

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([&](auto) { return flush_error; });
  });

  MockFactory mock_factory;
  // The implementation calls Resume on a permanent error.
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then(
        [&](auto) -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          return resume_error;
        });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // These flushes are issued without waiting and will be queued.
  auto f1 = connection->Flush(TestPayload(1));
  auto f2 = connection->Flush(TestPayload(1));
  ASSERT_FALSE(f1.is_ready());
  ASSERT_FALSE(f2.is_ready());

  // Let the flush complete with a permanent error.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  // This triggers the resume attempt.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  EXPECT_THAT(f1.get(), StatusIs(resume_error.code()));
  EXPECT_THAT(f2.get(), StatusIs(resume_error.code()));
}

TEST(WriteConnectionBuffered, FlushResumesAndDoesNotCompletePrematurely) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock1 = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock1, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock1, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  // The first flush fails, triggering resume.
  EXPECT_CALL(*mock1, Flush(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Flush1").then(
        [](auto) { return Status(StatusCode::kUnavailable, "try again"); });
  });

  auto mock2 = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock2, UploadId).WillRepeatedly(Return("test-upload-id"));
  // OnResume will query persisted state. We return 0, meaning the data was
  // lost.
  auto mock2_persisted_size = std::make_shared<std::int64_t>(0);
  EXPECT_CALL(*mock2, PersistedState).WillRepeatedly([mock2_persisted_size] {
    return MakePersistedState(*mock2_persisted_size);
  });
  // The resumed connection should receive the Flush call again.
  EXPECT_CALL(*mock2, Flush(expected_write_size(8 * 1024)))
      .WillOnce([&, mock2_persisted_size](auto) {
        return sequencer.PushBack("Flush2").then([mock2_persisted_size](auto) {
          *mock2_persisted_size = 8 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then([&](auto) {
      return make_status_or(
          std::unique_ptr<storage::AsyncWriterConnection>(std::move(mock2)));
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock1), TestOptions());

  auto f = connection->Flush(TestPayload(8 * 1024));
  ASSERT_FALSE(f.is_ready());

  // Let the first flush fail.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(true);

  // Trigger resume.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  // The flush promise must not be ready yet, because the data has not been
  // flushed on the new connection.
  ASSERT_FALSE(f.is_ready());

  // Let the second flush succeed.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);

  // Now the flush promise should be completed.
  ASSERT_TRUE(f.is_ready());
  EXPECT_STATUS_OK(f.get());
}

// Verifies that multiple queued Flush() operations track their
// respective cumulative byte offsets and that each flush future is satisfied
// only when the server's persisted_size reaches or exceeds its target offset.
TEST(WriteConnectionBuffered, InterleavedMultiFlush) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock_persisted_size = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });

  // Expect two 32 KiB flushes in sequence.
  EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush1").then([mock_persisted_size](auto) {
          *mock_persisted_size = 32 * 1024;
          return Status{};
        });
      })
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush2").then([mock_persisted_size](auto) {
          *mock_persisted_size = 64 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Issue first Flush() of 32 KiB.
  auto f1 = connection->Flush(TestPayload(32 * 1024));
  ASSERT_FALSE(f1.is_ready());

  // Issue second Flush() of 32 KiB while the first is still in-flight.
  auto f2 = connection->Flush(TestPayload(32 * 1024));
  ASSERT_FALSE(f2.is_ready());

  // Complete the first flush on the mock (persisted_size reaches 32 KiB).
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(true);

  // f1 must be satisfied with OK, while f2 must remain pending because its
  // target offset (64 KiB) has not yet been reached.
  ASSERT_TRUE(f1.is_ready());
  EXPECT_STATUS_OK(f1.get());
  ASSERT_FALSE(f2.is_ready());

  // Complete the second flush on the mock (persisted_size reaches 64 KiB).
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);

  // Now f2 must also be satisfied with OK.
  ASSERT_TRUE(f2.is_ready());
  EXPECT_STATUS_OK(f2.get());
}

TEST(WriteConnectionBuffered, FinalizeWhileFlushing) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));

  // An explicit 0-byte flush triggers a `Flush` on the underlying connection.
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto payload) {
    EXPECT_TRUE(payload.empty());
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  // Finalize should only be dispatched after Flush has completed.
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto payload) {
    EXPECT_TRUE(payload.empty());
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Trigger an explicit `Flush` with no data.
  auto flush = connection->Flush(storage::WritePayload{});
  ASSERT_FALSE(flush.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");

  // While `Flush` is in-flight, call `Finalize`.
  // Because the `writing_` flag remains true during `FlushStep`, `Finalize`
  // must be queued and must not call `mock->Finalize` concurrently.
  auto finalize = connection->Finalize(storage::WritePayload{});
  ASSERT_FALSE(finalize.is_ready());

  // Complete the `Flush` step.
  next.first.set_value(true);

  // `Flush` promise is satisfied.
  ASSERT_TRUE(flush.is_ready());
  EXPECT_STATUS_OK(flush.get());

  // Only now should `Finalize` be dispatched to the underlying connection.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  ASSERT_TRUE(finalize.is_ready());
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, WriteWhileFlushing) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));

  EXPECT_CALL(*mock, Flush).WillOnce([&](auto payload) {
    EXPECT_TRUE(payload.empty());
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Write).WillOnce([&](auto payload) {
    EXPECT_EQ(payload.size(), 1024);
    return sequencer.PushBack("Write").then([](auto) { return Status{}; });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto flush = connection->Flush(storage::WritePayload{});
  ASSERT_FALSE(flush.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");

  // Call `Write` while `Flush` is in-flight.
  auto write = connection->Write(TestPayload(1024));

  // Complete `Flush`.
  next.first.set_value(true);

  ASSERT_TRUE(flush.is_ready());
  EXPECT_STATUS_OK(flush.get());

  // Now `Write` should execute.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);

  EXPECT_STATUS_OK(write.get());
}

TEST(WriteConnectionBuffered, CloseEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
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

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto close = connection->Close({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);
  EXPECT_STATUS_OK(close.get());
}

TEST(WriteConnectionBuffered, DuplicateCloseFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([](auto) {
    return make_ready_future(Status{});
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto close1 = connection->Close({});
  auto close2 = connection->Close({});

  EXPECT_STATUS_OK(close1.get());
  EXPECT_THAT(close2.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionBuffered, CloseWithPayload) {
  AsyncSequencer<bool> sequencer;

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

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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

TEST(WriteConnectionBuffered, CloseFailsAndResumeFails) {
  AsyncSequencer<bool> sequencer;
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
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Retry").then([&](auto) {
      return StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
          resume_error);
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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

TEST(WriteConnectionBuffered, CloseFailsAndResumeSucceedsButNotClosed) {
  AsyncSequencer<bool> sequencer;
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
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then([](auto) {
      auto resumed_mock = std::make_unique<MockAsyncWriterConnection>();
      EXPECT_CALL(*resumed_mock, PersistedState)
          .WillRepeatedly(Return(MakePersistedState(0)));
      return make_status_or(std::unique_ptr<storage::AsyncWriterConnection>(
          std::move(resumed_mock)));
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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

TEST(WriteConnectionBuffered, CloseFailsAndResumeSucceedsAndFinalized) {
  AsyncSequencer<bool> sequencer;
  auto close_error = TransientError();
  auto write_error = TransientError();
  auto resume2_error = PermanentError();

  auto mock1 = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock1, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock1, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock1, Close).WillOnce([&](auto) {
    return sequencer.PushBack("Close").then(
        [close_error](auto) { return close_error; });
  });

  auto resumed_mock = std::make_unique<MockAsyncWriterConnection>();
  auto* resumed_mock_ptr = resumed_mock.get();

  // Mock Write on the resumed connection to fail, to trigger a second resume.
  EXPECT_CALL(*resumed_mock_ptr, Write).WillOnce([&](auto) {
    return sequencer.PushBack("Write2").then(
        [write_error](auto) { return write_error; });
  });

  MockFactory mock_factory;
  {
    InSequence seq;
    // The resume will succeed and return a new mock that reports the object is
    // already finalized (which implies closed).
    EXPECT_CALL(mock_factory, Call)
        .WillOnce([&, rm = std::move(resumed_mock)]() mutable {
          return sequencer.PushBack("Resume").then([rm = std::move(rm)](
                                                       auto) mutable {
            EXPECT_CALL(*rm, PersistedState)
                .WillRepeatedly(Return(TestObject()));
            return make_status_or(
                std::unique_ptr<storage::AsyncWriterConnection>(std::move(rm)));
          });
        });
    // The second resume (after write failure) will fail.
    EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
      return sequencer.PushBack("Resume2").then([resume2_error](auto) {
        return StatusOr<std::unique_ptr<storage::AsyncWriterConnection>>(
            resume2_error);
      });
    });
  }

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock1), TestOptions());

  auto close = connection->Close({});
  ASSERT_FALSE(close.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Close");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  EXPECT_STATUS_OK(close.get());

  // Write to the closed connection. Since HWM is 32KB, writing 16 bytes
  // returns OK immediately because it is buffered.
  auto w = connection->Write(TestPayload(16));
  ASSERT_TRUE(w.is_ready());
  EXPECT_STATUS_OK(w.get());

  // However, the background write loop will call resumed_mock->Write.
  // Let that background Write fail.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write2");
  next.first.set_value(true);

  // This triggers a second Resume. If resuming_ was not reset in SetClosed,
  // this would hang because Resume would return early and not call
  // mock_factory.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume2");
  next.first.set_value(true);

  // A subsequent write should fail immediately with the second resume error.
  auto w2 = connection->Write(TestPayload(16));
  ASSERT_TRUE(w2.is_ready());
  EXPECT_THAT(w2.get(), StatusIs(resume2_error.code()));
}

TEST(WriteConnectionBuffered, Query) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then(
        [](auto) { return StatusOr<std::int64_t>(TransientError()); });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto query = connection->Query();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
  next.first.set_value(true);
  ASSERT_TRUE(query.is_ready());
  EXPECT_THAT(query.get(), StatusIs(TransientError().code()));
}

TEST(WriteConnectionBuffered, GetRequestMetadata) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"k1", "v1"}, {"k1", "v2"}, {"k2", "v3"}},
                                   {{"t1", "v4"}, {"t2", "v5"}}}));

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  auto const actual = connection->GetRequestMetadata();
  EXPECT_THAT(actual.headers,
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k1", "v2"),
                                   Pair("k2", "v3")));
  EXPECT_THAT(actual.trailers,
              UnorderedElementsAre(Pair("t1", "v4"), Pair("t2", "v5")));
}

TEST(WriteConnectionBuffered, FlushSendsAllBufferedData) {
  AsyncSequencer<bool> sequencer;
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
  // The first Write() (8KiB) is immediately sent, not buffered.
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write-8K").then([](auto) { return Status{}; });
  });
  // The Flush() call has 4KiB of data. Since the buffer is empty, this
  // 4KiB is flushed.
  EXPECT_CALL(*mock, Flush(expected_write_size(4 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush-4K").then([mock_persisted_size](auto) {
          *mock_persisted_size = 8 * 1024 + 4 * 1024;
          return Status{};
        });
      });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Write some data. This is below the LWM, but it is sent immediately.
  auto w0 = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(w0.is_ready());
  EXPECT_STATUS_OK(w0.get());

  // Now flush, with a bit more data.
  auto f1 = connection->Flush(TestPayload(4 * 1024));
  ASSERT_FALSE(f1.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write-8K");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush-4K");
  next.first.set_value(true);

  EXPECT_STATUS_OK(f1.get());
}

TEST(WriteConnectionBuffered, FinalizeSendsAllBufferedData) {
  AsyncSequencer<bool> sequencer;
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
  // The first Write() (8KiB) is immediately sent, not buffered.
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write-8K").then([](auto) { return Status{}; });
  });
  // The Finalize() call has 4KiB of data. Since the buffer is empty, this
  // 4KiB is flushed.
  EXPECT_CALL(*mock, Flush(expected_write_size(4 * 1024)))
      .WillOnce([&, mock_persisted_size](auto) {
        return sequencer.PushBack("Flush-4K").then([mock_persisted_size](auto) {
          *mock_persisted_size = 8 * 1024 + 4 * 1024;
          return Status{};
        });
      });
  // After the flush, the buffer is empty, and we can finalize.
  EXPECT_CALL(*mock, Finalize(expected_write_size(0))).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Write some data. This is below the LWM, but it is sent immediately.
  auto w0 = connection->Write(TestPayload(8 * 1024));
  ASSERT_TRUE(w0.is_ready());
  EXPECT_STATUS_OK(w0.get());

  // Now finalize, with a bit more data.
  auto f1 = connection->Finalize(TestPayload(4 * 1024));
  ASSERT_FALSE(f1.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write-8K");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush-4K");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  EXPECT_THAT(f1.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, WriteTriggersMultipleFlushes) {
  AsyncSequencer<bool> sequencer;
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

  {
    InSequence seq;
    // Expect two separate flushes as the buffer fills up twice.
    EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024)))
        .WillOnce([&, mock_persisted_size](auto) {
          return sequencer.PushBack("Flush1").then([mock_persisted_size](auto) {
            *mock_persisted_size = 32 * 1024;
            return Status{};
          });
        });

    EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024)))
        .WillOnce([&, mock_persisted_size](auto) {
          return sequencer.PushBack("Flush2").then([mock_persisted_size](auto) {
            *mock_persisted_size = 64 * 1024;
            return Status{};
          });
        });
  }

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Write enough data to go over the HWM and block.
  auto w1 = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w1.is_ready());

  // Satisfy the first flush, which should unblock w1.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w1.get());

  // Write enough data again to go over the HWM and block.
  auto w2 = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w2.is_ready());

  // Satisfy the second flush, which should unblock w2.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w2.get());
}

TEST(WriteConnectionBuffered, MultipleConcurrentFlushesAreQueued) {
  AsyncSequencer<bool> sequencer;
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

  {
    InSequence seq;
    // The implementation sends the first Flush() payload immediately.
    EXPECT_CALL(*mock, Flush(expected_write_size(4096)))
        .WillOnce([&, mock_persisted_size](auto) {
          return sequencer.PushBack("Flush1").then([mock_persisted_size](auto) {
            *mock_persisted_size = 4096;
            return Status{};
          });
        });

    EXPECT_CALL(*mock, Flush(expected_write_size(8192)))
        .Times(1)
        .WillOnce([&, mock_persisted_size](auto) {
          return sequencer.PushBack("Flush2").then([mock_persisted_size](auto) {
            *mock_persisted_size = 4096 + 8192;
            return Status{};
          });
        });
  }

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Call Flush twice without waiting.
  auto f1 = connection->Flush(TestPayload(4096));
  auto f2 = connection->Flush(TestPayload(8192));
  ASSERT_FALSE(f1.is_ready());
  ASSERT_FALSE(f2.is_ready());

  // Satisfy the initial Flush(4k) call.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(true);

  // After the Query, the first Flush future should be completed.
  EXPECT_STATUS_OK(f1.get());

  // Satisfy the second Flush call.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);

  EXPECT_STATUS_OK(f2.get());
}

TEST(WriteConnectionBuffered, FinalizeWithEmptyPayloadIsImmediate) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));

  // With an empty buffer, Finalize({}) should immediately call the underlying
  // Finalize() without any intermediate Flush() or Query() calls.
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto payload) {
    EXPECT_TRUE(payload.empty());
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });

  // Ensure no Flush or Query is ever called in this scenario.
  EXPECT_CALL(*mock, Flush).Times(0);
  EXPECT_CALL(*mock, Query).Times(0);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto final_status = connection->Finalize({});
  ASSERT_FALSE(final_status.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  EXPECT_THAT(final_status.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, FinalizeFailsThenResumeFails) {
  AsyncSequencer<bool> sequencer;
  auto const transient_error = TransientError();
  auto const resume_error = PermanentError();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([&](auto) {
      return StatusOr<google::storage::v2::Object>(transient_error);
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then(
        [&](auto) -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          return resume_error;
        });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto finalize = connection->Finalize({});
  ASSERT_FALSE(finalize.is_ready());

  // Let the Finalize call fail.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  // This triggers the resume attempt, which will also fail.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  // The finalization should fail with the error from the resume attempt.
  EXPECT_THAT(finalize.get(), StatusIs(resume_error.code()));
}

TEST(WriteConnectionBuffered,
     FinalizeFailsThenResumeSucceedsButObjectStillMissing) {
  AsyncSequencer<bool> sequencer;
  auto const original_finalize_error = TransientError();

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([&](auto) {
      return StatusOr<google::storage::v2::Object>(original_finalize_error);
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then([](auto) {
      // The resume itself succeeds, but the new connection reports that the
      // object is still not finalized.
      auto resumed_mock = std::make_unique<MockAsyncWriterConnection>();
      EXPECT_CALL(*resumed_mock, PersistedState)
          .WillRepeatedly(Return(MakePersistedState(0)));
      return make_status_or(std::unique_ptr<storage::AsyncWriterConnection>(
          std::move(resumed_mock)));
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto finalize = connection->Finalize({});
  ASSERT_FALSE(finalize.is_ready());

  // Let the original finalize call fail.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);

  // Let the resume attempt succeed.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  // With the bug fixed, the logic enters the was_finalizing block and fails
  // with the original error. No retry is attempted.
  EXPECT_THAT(finalize.get(), StatusIs(original_finalize_error.code()));
}

TEST(WriteConnectionBuffered, SetFinalizedIsIdempotent) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));

  // This Finalize will succeed.
  EXPECT_CALL(*mock, Finalize).WillOnce([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return make_status_or(TestObject());
    });
  });
  // This Flush will fail, triggering a resume.
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then(
        [](auto) { return TransientError(); });
  });
  EXPECT_CALL(*mock, Query).Times(0);

  MockFactory mock_factory;
  // The resume attempt will discover the object is already finalized.
  EXPECT_CALL(mock_factory, Call).WillOnce([&] {
    return sequencer.PushBack("Resume").then(
        [](auto) -> StatusOr<std::unique_ptr<storage::AsyncWriterConnection>> {
          auto resumed_mock = std::make_unique<MockAsyncWriterConnection>();
          EXPECT_CALL(*resumed_mock, PersistedState)
              .WillRepeatedly(Return(TestObject()));
          return std::unique_ptr<storage::AsyncWriterConnection>(
              std::move(resumed_mock));
        });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Step 1: Successfully finalize the connection.
  auto finalize = connection->Finalize({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(true);
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));

  // The final promise is now completed. The idempotency guard should be active.

  // Step 2: Start a *new* operation that fails and triggers a resume.
  auto flush = connection->Flush({});

  // Step 3: Let the Flush operation fail.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);  // This makes it return TransientError

  // Step 4: The failure triggers a resume. Let the resume succeed.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);
}

TEST(WriteConnectionBuffered, ResetWriteOffsetOnResume) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto* mock_ptr = mock.get();

  EXPECT_CALL(*mock_ptr, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock_ptr, PersistedState)
      .WillOnce(
          Return(MakePersistedState(0)));  // Initial state: 0 bytes persisted.

  EXPECT_CALL(*mock_ptr, Write).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then([](auto f) {
      if (!f.get()) return TransientError();  // This write will fail.
      return Status{};
    });
  });

  MockFactory mock_factory;
  auto resumed_mock = std::make_unique<MockAsyncWriterConnection>();
  auto* resumed_mock_ptr = resumed_mock.get();

  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Resume").then([&](auto) {
      // The resumed connection reports that 1024 bytes have been persisted.
      EXPECT_CALL(*resumed_mock_ptr, PersistedState)
          .WillRepeatedly(Return(MakePersistedState(1024)));
      // We expect the next write on the resumed stream to send the remaining
      // 1024 bytes. If the write offset was not reset to 0, this size would be
      // incorrect.
      EXPECT_CALL(*resumed_mock_ptr, Write).WillOnce([&](auto payload) {
        EXPECT_EQ(payload.size(), 1024);
        return sequencer.PushBack("ResumedWrite").then([](auto) {
          return Status{};
        });
      });
      return make_status_or(std::unique_ptr<storage::AsyncWriterConnection>(
          std::move(resumed_mock)));
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  // Write a total of 2048 bytes.
  auto write = connection->Write(TestPayload(2048));

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Resume");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "ResumedWrite");
  next.first.set_value(true);

  EXPECT_STATUS_OK(write.get());
}

TEST(WriteConnectionBuffered, CloseAfterFinalizeFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([](auto) {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto finalize = connection->Finalize({});
  auto close = connection->Close({});

  EXPECT_STATUS_OK(finalize.get());
  EXPECT_THAT(close.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionBuffered, FinalizeAfterCloseFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Close).WillOnce([](auto) {
    return make_ready_future(Status{});
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto close = connection->Close({});
  auto finalize = connection->Finalize({});

  EXPECT_STATUS_OK(close.get());
  EXPECT_THAT(finalize.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionBuffered, DuplicateFinalizeFails) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Finalize).WillOnce([](auto) {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });

  MockFactory mock_factory;
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto finalize1 = connection->Finalize({});
  auto finalize2 = connection->Finalize({});

  EXPECT_STATUS_OK(finalize1.get());
  EXPECT_THAT(finalize2.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionBuffered, PermanentErrorNoResume) {
  AsyncSequencer<bool> sequencer;
  auto failed_precondition =
      Status(StatusCode::kFailedPrecondition, "precondition failed");

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Write).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then(
        [failed_precondition](auto) { return failed_precondition; });
  });

  MockFactory mock_factory;
  // factory_ should NOT be called because error is permanent
  // (FAILED_PRECONDITION).
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

  auto write1 = connection->Write(TestPayload(1));
  EXPECT_STATUS_OK(write1.get());

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, Eq("Write"));
  next.first.set_value(true);

  auto write2 = connection->Write(TestPayload(1));
  EXPECT_THAT(write2.get(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST(WriteConnectionBuffered, CustomRetryPolicyOption) {
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
  auto invalid_argument =
      Status(StatusCode::kInvalidArgument, "custom permanent error");

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Write).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then(
        [invalid_argument](auto) { return invalid_argument; });
  });

  MockFactory mock_factory;
  // factory_ should NOT be called because error is permanent per custom policy.
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto options = TestOptions().set<storage::AsyncRetryPolicyOption>(
      std::make_shared<CustomAsyncRetryPolicy>());

  auto connection = MakeWriterConnectionBuffered(mock_factory.AsStdFunction(),
                                                 std::move(mock), options);

  auto write1 = connection->Write(TestPayload(1));
  EXPECT_STATUS_OK(write1.get());

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, Eq("Write"));
  next.first.set_value(true);

  auto write2 = connection->Write(TestPayload(1));
  EXPECT_THAT(write2.get(), StatusIs(StatusCode::kInvalidArgument));
}

/// Test case for an operation issued from inside a `Flush()` continuation.
struct ChainedOpCase {
  std::string name;
  std::function<future<Status>(storage::AsyncWriterConnection&)> start_op;
  std::string expected_call;
};

class WriterConnectionBufferedFlushCallbackTest
    : public ::testing::TestWithParam<ChainedOpCase> {};

/// @test Verify that an operation chained from a `Flush()` continuation is
/// dispatched to the underlying connection before the continuation returns.
TEST_P(WriterConnectionBufferedFlushCallbackTest,
       ChainedOpDispatchedBeforeCallbackReturns) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls, "");
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
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
    WriteConnectionBuffered, WriterConnectionBufferedFlushCallbackTest,
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
/// another thread does not race with `OnQuery()` on `writing_`.
TEST(WriteConnectionBuffered, FlushCallbackCrossThreadFlush) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto mock_persisted_size = std::make_shared<std::int64_t>(0);

  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
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
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, FlushFuturesCompleteInOrderWithInlineCompletion) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto mock_persisted_size = std::make_shared<std::int64_t>(0);

  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly([mock_persisted_size] {
    return MakePersistedState(*mock_persisted_size);
  });
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
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, SmallWriteDoesNotCancelQueuedFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls, "");
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, ChainedSmallWriteDoesNotCancelQueuedFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls, "");
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, HwmWaitersStayBlockedUntilBelowLwm) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls, "");
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, HwmWriteCallbackChainedOpDispatchedBeforeReturn) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls, "");
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, HwmWriteCallbackChainedOpDispatchedAfterResume) {
  AsyncSequencer<bool> sequencer;
  auto persisted1 = std::make_shared<std::int64_t>(0);
  auto calls1 = std::make_shared<int>(0);
  auto mock1 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock1, sequencer, persisted1, calls1, "1");
  // The resumed connection reports that all 32 KiB were persisted.
  auto persisted2 = std::make_shared<std::int64_t>(32 * 1024);
  auto calls2 = std::make_shared<int>(0);
  auto mock2 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock2, sequencer, persisted2, calls2, "2");

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&] {
    return make_ready_future(make_status_or(
        std::unique_ptr<storage::AsyncWriterConnection>(std::move(mock2))));
  });
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock1), TestOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  future<Status> chained;
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained = connection->Write(TestPayload(1024));
    EXPECT_EQ(*calls2, 1);
  });

  // Fail the flush to trigger a `Resume()`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(false);
  ASSERT_TRUE(callback_done.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write2");
  next.first.set_value(true);
  EXPECT_STATUS_OK(chained.get());
}

/// @test Verify that a small `Write()` chained from a high-water mark
/// continuation during a `Resume()` does not cancel a pending `Flush()`.
TEST(WriteConnectionBuffered, ChainedSmallWriteAfterResumeKeepsPendingFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted1 = std::make_shared<std::int64_t>(0);
  auto calls1 = std::make_shared<int>(0);
  auto mock1 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock1, sequencer, persisted1, calls1, "1");
  auto persisted2 = std::make_shared<std::int64_t>(32 * 1024);
  auto calls2 = std::make_shared<int>(0);
  auto mock2 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock2, sequencer, persisted2, calls2, "2");

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&] {
    return make_ready_future(make_status_or(
        std::unique_ptr<storage::AsyncWriterConnection>(std::move(mock2))));
  });
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock1), TestOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  auto flush = connection->Flush(TestPayload(1024));
  future<Status> chained;
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    chained = connection->Write(TestPayload(1024));
  });

  // Fail the flush. The resumed connection reports 32 KiB persisted, leaving
  // 1 KiB (below the low-water mark) buffered for the pending `Flush()`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(false);
  ASSERT_TRUE(callback_done.is_ready());
  ASSERT_FALSE(flush.is_ready());

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);
  ASSERT_TRUE(flush.is_ready());
  EXPECT_STATUS_OK(flush.get());
  EXPECT_STATUS_OK(chained.get());
}

/// @test Verify that a `Resume()` does not release `Write()` calls blocked at
/// the high-water mark while the buffer is still above the low-water mark.
TEST(WriteConnectionBuffered, HwmWaitersStayBlockedAfterResumeUntilBelowLwm) {
  AsyncSequencer<bool> sequencer;
  auto persisted1 = std::make_shared<std::int64_t>(0);
  auto calls1 = std::make_shared<int>(0);
  auto mock1 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock1, sequencer, persisted1, calls1, "1");
  // The resumed connection reports that nothing was persisted.
  auto persisted2 = std::make_shared<std::int64_t>(0);
  auto calls2 = std::make_shared<int>(0);
  auto mock2 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock2, sequencer, persisted2, calls2, "2");

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&] {
    return make_ready_future(make_status_or(
        std::unique_ptr<storage::AsyncWriterConnection>(std::move(mock2))));
  });
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock1), TestOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());

  // Fail the flush to trigger a `Resume()`. All 32 KiB are still buffered,
  // which is above the 16 KiB low-water mark, so `w` must stay blocked.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush1");
  next.first.set_value(false);
  EXPECT_FALSE(w.is_ready());

  // Once the resumed connection persists the data, `w` is released.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush2");
  next.first.set_value(true);
  ASSERT_TRUE(w.is_ready());
  EXPECT_STATUS_OK(w.get());
}

/// @test Verify that, with a zero low-water mark, a `Write()` blocked at the
/// high-water mark is released once the buffer drains, and that the writer
/// does not keep issuing empty flushes afterwards.
TEST(WriteConnectionBuffered, HwmWaiterReleasedWhenBufferDrainsWithZeroLwm) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock, sequencer, persisted, calls, "");
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock),
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
TEST(WriteConnectionBuffered, ChainedInlineFlushDoesNotOvertakeEarlierFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
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
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered,
     HwmWaiterChainedInlineFlushDoesNotOvertakePendingFlush) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
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
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
TEST(WriteConnectionBuffered, ChainedOpPermanentErrorDoesNotRestartWriteLoop) {
  AsyncSequencer<bool> sequencer;
  auto persisted = std::make_shared<std::int64_t>(0);
  auto write_calls = std::make_shared<int>(0);
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
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
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());

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
/// the failed connection.
TEST(WriteConnectionBuffered,
     ChainedOpPermanentErrorAfterResumeDoesNotRestartWriteLoop) {
  AsyncSequencer<bool> sequencer;
  auto persisted1 = std::make_shared<std::int64_t>(0);
  auto calls1 = std::make_shared<int>(0);
  auto mock1 = std::make_unique<MockAsyncWriterConnection>();
  RecordWritesAndFlushes(*mock1, sequencer, persisted1, calls1, "1");
  // Reporting all data persisted releases `w` as part of the resume itself;
  // the chained `Write()` then fails inline.
  auto write_calls2 = std::make_shared<int>(0);
  auto mock2 = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock2, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock2, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(32 * 1024)));
  EXPECT_CALL(*mock2, Write)
      .WillRepeatedly([write_calls2](storage::WritePayload const&) {
        ++*write_calls2;
        return make_ready_future(PermanentError());
      });
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&] {
    return make_ready_future(make_status_or(
        std::unique_ptr<storage::AsyncWriterConnection>(std::move(mock2))));
  });
  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock1), TestOptions());

  auto w = connection->Write(TestPayload(32 * 1024));
  ASSERT_FALSE(w.is_ready());
  auto callback_done = w.then([&](future<Status> f) {
    EXPECT_STATUS_OK(f.get());
    (void)connection->Write(TestPayload(1024));
  });

  // A failed flush triggers the `Resume()`.
  auto next = sequencer.PopFrontWithName();
  ASSERT_THAT(next.second, Eq("Flush1"));
  next.first.set_value(false);
  ASSERT_TRUE(callback_done.is_ready());
  EXPECT_THAT(*write_calls2, Eq(1));
  EXPECT_THAT(connection->Write(TestPayload(1)).get(),
              StatusIs(PermanentError().code()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
