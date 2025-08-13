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
#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/storage/v2/storage.pb.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;
using ::testing::VariantWith;

using MockFactory =
    ::testing::MockFunction<future<StatusOr<WriteObject::WriteResult>>(
        google::storage::v2::BidiWriteObjectRequest)>;

using MockStreamingRpc =
    ::testing::MockFunction<std::unique_ptr<WriteObject::StreamingRpc>()>;

absl::variant<std::int64_t, google::storage::v2::Object> MakePersistedState(
    std::int64_t persisted_size) {
  return persisted_size;
}

storage_experimental::WritePayload TestPayload(std::size_t n) {
  return storage_experimental::WritePayload(std::string(n, 'A'));
}

auto TestObject() {
  auto object = google::storage::v2::Object{};
  object.set_bucket("projects/_/buckets/test-bucket");
  object.set_name("test-object");
  return object;
}

TEST(WriteConnectionResumed, FinalizeEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};

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

  auto connection =
      MakeWriterConnectionResumed(mock_factory.AsStdFunction(), std::move(mock),
                                  initial_request, nullptr, Options{});
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
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, UploadId).WillRepeatedly(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillRepeatedly(Return(TestObject()));
  EXPECT_CALL(*mock, Finalize).Times(0);

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection =
      MakeWriterConnectionResumed(mock_factory.AsStdFunction(), std::move(mock),
                                  initial_request, nullptr, Options{});
  EXPECT_EQ(connection->UploadId(), "test-upload-id");
  EXPECT_THAT(
      connection->PersistedState(),
      VariantWith<google::storage::v2::Object>(IsProtoEqual(TestObject())));

  auto finalize = connection->Finalize({});
  EXPECT_TRUE(finalize.is_ready());
  EXPECT_THAT(finalize.get(), IsOkAndHolds(IsProtoEqual(TestObject())));
}

TEST(WriteConnectionResumed, Cancel) {
  AsyncSequencer<bool> sequencer;
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
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

  auto connection =
      MakeWriterConnectionResumed(mock_factory.AsStdFunction(), std::move(mock),
                                  initial_request, nullptr, Options{});

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

  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto const& p) {
    EXPECT_TRUE(p.payload().empty());
    return sequencer.PushBack("Flush").then([](auto f) {
      if (!f.get()) return TransientError();
      return Status{};
    });
  });
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then(
        [](auto f) -> StatusOr<std::int64_t> {
          if (!f.get()) return TransientError();
          return 0;
        });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection =
      MakeWriterConnectionResumed(mock_factory.AsStdFunction(), std::move(mock),
                                  initial_request, nullptr, Options{});
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto flush = connection->Flush({});
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
  next.first.set_value(true);

  EXPECT_THAT(flush.get(), StatusIs(StatusCode::kOk));
}

TEST(WriteConnectionResumed, FlushNonEmpty) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  auto initial_request = google::storage::v2::BidiWriteObjectRequest{};
  auto const payload = TestPayload(1024);

  EXPECT_CALL(*mock, PersistedState)
      .WillRepeatedly(Return(MakePersistedState(0)));
  EXPECT_CALL(*mock, Flush).WillOnce([&](auto const& p) {
    EXPECT_EQ(p.payload(), payload.payload());
    return sequencer.PushBack("Flush").then([](auto f) {
      if (!f.get()) return TransientError();
      return Status{};
    });
  });
  EXPECT_CALL(*mock, Query).WillOnce([&]() {
    return sequencer.PushBack("Query").then(
        [](auto f) -> StatusOr<std::int64_t> {
          if (!f.get()) return TransientError();
          return 1024;
        });
  });

  MockFactory mock_factory;
  EXPECT_CALL(mock_factory, Call).Times(0);

  auto connection =
      MakeWriterConnectionResumed(mock_factory.AsStdFunction(), std::move(mock),
                                  initial_request, nullptr, Options{});
  EXPECT_THAT(connection->PersistedState(), VariantWith<std::int64_t>(0));

  auto write = connection->Write(payload);
  ASSERT_FALSE(write.is_ready());

  auto flush = connection->Flush({});
  ASSERT_FALSE(flush.is_ready());

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Flush");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Query");
  next.first.set_value(true);

  EXPECT_TRUE(flush.is_ready());
  EXPECT_THAT(flush.get(), StatusIs(StatusCode::kOk));

  EXPECT_TRUE(write.is_ready());
  EXPECT_THAT(write.get(), StatusIs(StatusCode::kOk));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
