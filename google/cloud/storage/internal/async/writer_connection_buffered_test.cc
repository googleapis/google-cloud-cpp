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
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage_experimental::AsyncWriterConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using MockFactory = ::testing::MockFunction<future<
    StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>()>;

Options TestOptions() {
  return Options{}
      .set<storage_experimental::BufferedUploadLwmOption>(16 * 1024)
      .set<storage_experimental::BufferedUploadHwmOption>(32 * 1024);
}

absl::variant<std::int64_t, google::storage::v2::Object> MakePersistedState(
    std::int64_t persisted_size) {
  return persisted_size;
}

auto TestObject() {
  auto object = google::storage::v2::Object{};
  object.set_bucket("projects/_/buckets/test-bucket");
  object.set_name("test-object");
  return object;
}

storage_experimental::WritePayload TestPayload(std::size_t n) {
  return storage_experimental::WritePayload(std::string(n, 'A'));
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

TEST(WriteConnectionBuffered, FinalizeEmptyResumes) {
  AsyncSequencer<bool> sequencer;
  auto make_mock = [&sequencer]() -> std::unique_ptr<AsyncWriterConnection> {
    auto mock = std::make_unique<MockAsyncWriterConnection>();
    EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
    EXPECT_CALL(*mock, PersistedState)
        .WillRepeatedly(Return(MakePersistedState(0)));
    EXPECT_CALL(*mock, Finalize).Times(AtMost(1)).WillRepeatedly([&](auto) {
      return sequencer.PushBack("Finalize")
          .then([](auto f) -> StatusOr<google::storage::v2::Object> {
            if (!f.get()) return TransientError();
            return TestObject();
          });
    });
    return mock;
  };

  auto mock = make_mock();
  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Retry").then(
        [&](auto) { return make_status_or(make_mock()); });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto finalize = connection->Finalize({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Retry");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
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
  EXPECT_CALL(*mock, Finalize).Times(AtMost(1)).WillRepeatedly([&](auto) {
    return sequencer.PushBack("Finalize").then([](auto) {
      return StatusOr<google::storage::v2::Object>(TransientError());
    });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).WillOnce([&]() {
    return sequencer.PushBack("Retry").then([](auto) {
      auto mock = std::make_unique<MockAsyncWriterConnection>();
      EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
      EXPECT_CALL(*mock, PersistedState).WillRepeatedly(Return(TestObject()));
      return make_status_or(
          std::unique_ptr<AsyncWriterConnection>(std::move(mock)));
    });
  });

  auto connection = MakeWriterConnectionBuffered(
      mock_factory.AsStdFunction(), std::move(mock), TestOptions());
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto finalize = connection->Finalize({});
  EXPECT_FALSE(finalize.is_ready());
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finalize");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Retry");
  next.first.set_value(true);

  EXPECT_TRUE(finalize.is_ready());
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionBuffered, WriteResumes) {
  AsyncSequencer<bool> sequencer;
  auto make_mock = [&sequencer]() -> std::unique_ptr<AsyncWriterConnection> {
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

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(24 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then([](auto) {
      return make_status_or(static_cast<std::int64_t>(32 * 1024));
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
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
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

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(24 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then([](auto) {
      return make_status_or(static_cast<std::int64_t>(24 * 1024));
    });
  });
  EXPECT_CALL(*mock, Finalize(expected_write_size(32 * 1024)))
      .WillOnce([&](auto) {
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
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
  next.first.set_value(true);

  // That should free enough data in the buffer to continue writing.
  ASSERT_TRUE(w1.is_ready());
  EXPECT_STATUS_OK(w1.get());

  // Assume there is now a Finalize() call.
  auto finalize = connection->Finalize(TestPayload(32 * 1024));
  EXPECT_FALSE(finalize.is_ready());

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
      return StatusOr<std::unique_ptr<AsyncWriterConnection>>(TransientError());
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

TEST(WriteConnectionBuffered, RewindError) {
  AsyncSequencer<bool> sequencer;

  auto expected_write_size = [](std::size_t n) {
    return ResultOf(
        "payload size", [](auto payload) { return payload.size(); }, Eq(n));
  };

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(32 * 1024)));
  EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  // This should not happen: the service is rewinding the value of
  // `persisted_size`. The client should report this error.
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then([](auto) {
      return make_status_or(static_cast<std::int64_t>(16 * 1024));
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
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
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

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(32 * 1024)));
  EXPECT_CALL(*mock, Flush(expected_write_size(32 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  // This should not happen: the service is reporting more data persisted than
  // the size of the data sent.  The client should report this error.
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then([](auto) {
      return make_status_or(static_cast<std::int64_t>(128 * 1024));
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
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
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
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Write(expected_write_size(8 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Write").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Flush(expected_write_size(24 * 1024))).WillOnce([&](auto) {
    return sequencer.PushBack("Flush").then([](auto) { return Status{}; });
  });
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then([](auto) {
      return make_status_or(static_cast<std::int64_t>(32 * 1024));
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
  auto w0 = connection->Flush(TestPayload(8 * 1024));
  ASSERT_TRUE(w0.is_ready());
  EXPECT_STATUS_OK(w0.get());

  // The first connection->Write() should trigger a Write() request to drain the
  // buffer.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");

  auto w1 = connection->Flush(TestPayload(8 * 1024));
  ASSERT_TRUE(w1.is_ready());
  EXPECT_STATUS_OK(w1.get());
  auto w2 = connection->Flush(TestPayload(8 * 1024));
  ASSERT_TRUE(w2.is_ready());
  EXPECT_STATUS_OK(w2.get());

  // This blocks as it fills the buffer.
  auto w3 = connection->Flush(TestPayload(8 * 1024));
  ASSERT_FALSE(w3.is_ready());

  // Satisfying the first Write() call should trigger a second one.
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
  next.first.set_value(true);

  ASSERT_TRUE(w3.is_ready());
  EXPECT_STATUS_OK(w3.get());
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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
