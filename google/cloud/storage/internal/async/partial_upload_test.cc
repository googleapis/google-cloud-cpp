// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/async/partial_upload.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Request = google::storage::v2::BidiWriteObjectRequest;
using Response = google::storage::v2::BidiWriteObjectResponse;
using MockStream =
    ::google::cloud::mocks::MockAsyncStreamingReadWriteRpc<Request, Response>;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;

auto constexpr kExpectedChunkSize = 2 * 1024 * 1024L;

std::string RandomData(google::cloud::internal::DefaultPRNG& generator,
                       std::size_t size) {
  return google::cloud::internal::Sample(
      generator, static_cast<int>(size),
      "abcdefghijklmnopqrstuvwxyz0123456789");
}

TEST(PartialUpload, FinalizeEmptyWithoutChecksum) {
  AsyncSequencer<bool> sequencer;

  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&sequencer](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_append_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_TRUE(request.finish_write());
        EXPECT_FALSE(request.has_object_checksums());
        EXPECT_EQ(request.checksummed_data().crc32c(), 0);
        EXPECT_EQ(request.object_checksums().crc32c(), 0);
        EXPECT_TRUE(wopt.is_last_message());
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(), PartialUpload::kFinalize);
  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, FinalizeEmpty) {
  AsyncSequencer<bool> sequencer;

  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&sequencer](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_TRUE(request.finish_write());
        EXPECT_TRUE(request.has_object_checksums());
        EXPECT_EQ(request.checksummed_data().crc32c(), 0);
        EXPECT_EQ(request.object_checksums().crc32c(), 0);
        EXPECT_TRUE(wopt.is_last_message());
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call =
      PartialUpload::Call(std::move(rpc), std::move(hash), request,
                          absl::Cord(), PartialUpload::kFinalizeWithChecksum);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, FinalizeChunkAligned) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, 2 * kExpectedChunkSize);
  auto const view = absl::string_view(buffer.data(), buffer.size());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(0, kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_TRUE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_TRUE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(kExpectedChunkSize)));
        EXPECT_TRUE(request.has_object_checksums());
        EXPECT_EQ(request.object_checksums().crc32c(), Crc32c(view));
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(buffer),
                                  PartialUpload::kFinalizeWithChecksum);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, FinalizeChunkPartial) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, 2 * kExpectedChunkSize + 1024);
  auto const view = absl::string_view(buffer.data(), buffer.size());
  auto const v0 = view.substr(0, kExpectedChunkSize);
  auto const v1 = view.substr(kExpectedChunkSize, kExpectedChunkSize);
  auto const v2 = view.substr(2 * kExpectedChunkSize);

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(v0));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(v1));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_TRUE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 2 * kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_TRUE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(v2));
        EXPECT_TRUE(request.has_object_checksums());
        EXPECT_EQ(request.object_checksums().crc32c(), Crc32c(view));
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(buffer),
                                  PartialUpload::kFinalizeWithChecksum);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, FlushChunkEmpty) {
  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), 0);
        EXPECT_FALSE(request.has_object_checksums());
        EXPECT_TRUE(request.flush());
        EXPECT_TRUE(request.state_lookup());
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(), PartialUpload::kFlush);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, FlushChunkAligned) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, kExpectedChunkSize);
  auto const view = absl::string_view(buffer.data(), buffer.size());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(view));
        EXPECT_FALSE(request.has_object_checksums());
        EXPECT_TRUE(request.flush());
        EXPECT_TRUE(request.state_lookup());
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(buffer), PartialUpload::kFlush);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, FlushChunkPartial) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, 2 * kExpectedChunkSize + 1024);
  auto const view = absl::string_view(buffer.data(), buffer.size());
  auto const v0 = view.substr(0, kExpectedChunkSize);
  auto const v1 = view.substr(kExpectedChunkSize, kExpectedChunkSize);
  auto const v2 = view.substr(2 * kExpectedChunkSize);

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(v0));
        EXPECT_FALSE(request.has_object_checksums());
        EXPECT_FALSE(request.flush());
        EXPECT_FALSE(request.state_lookup());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(v1));
        EXPECT_FALSE(request.has_object_checksums());
        EXPECT_FALSE(request.flush());
        EXPECT_FALSE(request.state_lookup());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 2 * kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(), Crc32c(v2));
        EXPECT_FALSE(request.has_object_checksums());
        EXPECT_TRUE(request.flush());
        EXPECT_TRUE(request.state_lookup());
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(buffer), PartialUpload::kFlush);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, NotFinalizeChunkPartial) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const buffer = RandomData(generator, 2 * kExpectedChunkSize + 1024);
  auto const view = absl::string_view(buffer.data(), buffer.size());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write)
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 0);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_TRUE(request.has_upload_id());
        EXPECT_EQ(request.upload_id(), "test-upload-id");
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(0, kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(kExpectedChunkSize, kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      })
      .WillOnce([&](Request const& request, grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        EXPECT_EQ(request.write_offset(), 2 * kExpectedChunkSize);
        EXPECT_FALSE(request.has_write_object_spec());
        EXPECT_FALSE(request.has_upload_id());
        EXPECT_FALSE(request.finish_write());
        EXPECT_EQ(request.checksummed_data().crc32c(),
                  Crc32c(view.substr(2 * kExpectedChunkSize)));
        EXPECT_FALSE(request.has_object_checksums());
        return sequencer.PushBack("Write");
      });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(std::move(rpc), std::move(hash), request,
                                  absl::Cord(buffer), PartialUpload::kNone);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(true));
}

TEST(PartialUpload, ErrorOnWrite) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Write).Times(2).WillRepeatedly([&sequencer] {
    return sequencer.PushBack("Write");
  });

  auto hash = std::make_unique<storage::internal::Crc32cHashFunction>();
  Request request;
  request.set_upload_id("test-upload-id");
  auto call = PartialUpload::Call(
      std::move(rpc), std::move(hash), request,
      absl::Cord(RandomData(generator, 2 * kExpectedChunkSize)),
      PartialUpload::kNone);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Write");
  next.first.set_value(false);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, IsOkAndHolds(false));
}

TEST(PartialUpload, ErrorOnChecksums) {
  AsyncSequencer<bool> sequencer;
  auto rpc = std::make_unique<MockStream>();
  EXPECT_CALL(*rpc, Cancel).WillOnce([&sequencer] {
    return sequencer.PushBack("Cancel");
  });

  auto hash = std::make_unique<storage::internal::PrecomputedHashFunction>(
      storage::internal::HashValues{"invalid", ""});
  Request request;
  request.set_upload_id("test-upload-id");
  auto call =
      PartialUpload::Call(std::move(rpc), std::move(hash), request,
                          absl::Cord(), PartialUpload::kFinalizeWithChecksum);

  auto result = call->Start();

  auto next = sequencer.PopFrontWithName();
  EXPECT_THAT(next.second, "Cancel");
  next.first.set_value(true);

  ASSERT_TRUE(result.is_ready());
  auto success = result.get();
  EXPECT_THAT(success, StatusIs(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
