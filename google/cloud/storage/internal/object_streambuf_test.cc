// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/object_streambuf.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

/// @test Verify that uploading an empty stream creates a single chunk.
TEST(ObjectWriteStreambufTest, EmptyStream) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;

  int count = 0;
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_TRUE(p.empty());
        EXPECT_EQ(0, s);
        return make_status_or(ResumableUploadResponse{
            "{}", 0, {}, ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(0));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that uploading a small stream creates a single chunk.
TEST(ObjectWriteStreambufTest, SmallStream) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = "small test payload";

  int count = 0;
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_EQ(payload, p);
        EXPECT_EQ(payload.size(), s);
        auto last_committed_byte = payload.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(0));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload.data(), payload.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that uploading a stream which ends on a upload chunk quantum
/// works as expected.
TEST(ObjectWriteStreambufTest, EmptyTrailer) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '*');

  int count = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    EXPECT_EQ(payload, p);
    auto last_committed_byte = payload.size() - 1;
    return make_status_or(ResumableUploadResponse{
        "", last_committed_byte, {}, ResumableUploadResponse::kInProgress});
  }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        EXPECT_TRUE(p.empty());
        EXPECT_EQ(quantum, s);
        auto last_committed_byte = quantum - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(quantum));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload.data(), payload.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream sends a single message for large payloads.
TEST(ObjectWriteStreambufTest, FlushAfterLargePayload) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload_1(3 * quantum, '*');
  std::string const payload_2("trailer");

  int count = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    EXPECT_EQ(payload_1, p);
    auto last_commited_byte = p.size() - 1;
    return make_status_or(ResumableUploadResponse{
        "", last_commited_byte, {}, ResumableUploadResponse::kInProgress});
  }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        EXPECT_EQ(payload_2, p);
        EXPECT_EQ(payload_1.size() + payload_2.size(), s);
        auto last_committed_byte = payload_1.size() + payload_2.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(3 * quantum));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload_1.data(), payload_1.size());
  streambuf.sputn(payload_2.data(), payload_2.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when a full quantum is available.
TEST(ObjectWriteStreambufTest, FlushAfterFullQuantum) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload_1("header");
  std::string const payload_2(quantum, '*');

  int count = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    auto expected = payload_1 + payload_2.substr(0, quantum - payload_1.size());
    EXPECT_EQ(expected, p);
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress});
  }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        auto expected = payload_2.substr(payload_2.size() - payload_1.size());
        EXPECT_EQ(expected, p);
        EXPECT_EQ(payload_1.size() + payload_2.size(), s);
        auto last_committed_byte = payload_1.size() + payload_2.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(quantum));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload_1.data(), payload_1.size());
  streambuf.sputn(payload_2.data(), payload_2.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when adding one character at a time.
TEST(ObjectWriteStreambufTest, OverflowFlushAtFullQuantum) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '*');

  int count = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    auto expected = payload;
    EXPECT_EQ(expected, p);
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress});
  }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        EXPECT_TRUE(p.empty());
        EXPECT_EQ(payload.size(), s);
        auto last_committed_byte = payload.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(quantum));
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  for (auto const& c : payload) {
    streambuf.sputc(c);
  }
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when mixing operations that add one
/// character at a time and operations that add buffers.
TEST(ObjectWriteStreambufTest, MixPutcPutn) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload_1("header");
  std::string const payload_2(quantum, '*');

  int count = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    auto expected = payload_1 + payload_2.substr(0, quantum - payload_1.size());
    EXPECT_EQ(expected, p);
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress});
  }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        auto expected = payload_2.substr(payload_2.size() - payload_1.size());
        EXPECT_EQ(expected, p);
        EXPECT_EQ(payload_1.size() + payload_2.size(), s);
        auto last_committed_byte = payload_1.size() + payload_2.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(quantum));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  for (auto const& c : payload_1) {
    streambuf.sputc(c);
  }
  streambuf.sputn(payload_2.data(), payload_2.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream created for a finished upload starts out as
/// closed.
TEST(ObjectWriteStreambufTest, CreatedForFinalizedUpload) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(true));
  std::string const payload = "payload";
  auto last_upload_response = make_status_or(ResumableUploadResponse{
      "{}", 0, payload, ResumableUploadResponse::kDone});
  EXPECT_CALL(*mock, last_response).WillOnce(ReturnRef(last_upload_response));

  ObjectWriteStreambuf streambuf(
      std::move(mock), UploadChunkRequest::kChunkSizeQuantum,
      google::cloud::internal::make_unique<NullHashValidator>());
  EXPECT_EQ(streambuf.IsOpen(), false);
  StatusOr<HttpResponse> close_result = streambuf.Close();
  ASSERT_STATUS_OK(close_result);
  EXPECT_EQ(close_result.value().status_code, 200);
  EXPECT_EQ(close_result.value().payload, payload);
}

/// @test Verify that last error status is accessible for small payload.
TEST(ObjectWriteStreambufTest, ErroneousStream) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = "small test payload";

  int count = 0;
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t n) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_EQ(payload, p);
        EXPECT_EQ(payload.size(), n);
        return Status(StatusCode::kInvalidArgument, "Invalid Argument");
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(0));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload.data(), payload.size());
  auto response = streambuf.Close();

  EXPECT_EQ(StatusCode::kInvalidArgument, response.status().code())
      << ", status=" << response.status();
  EXPECT_EQ(StatusCode::kInvalidArgument, streambuf.last_status().code())
      << ", status=" << streambuf.last_status();
}

/// @test Verify that last error status is accessible for large payloads.
TEST(ObjectWriteStreambufTest, ErrorInLargePayload) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload_1(3 * quantum, '*');
  std::string const payload_2("trailer");

  int count = 0;
  EXPECT_CALL(*mock, UploadChunk(_))
      .WillOnce(Invoke([&](std::string const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_EQ(payload_1, p);
        return Status(StatusCode::kInvalidArgument, "Invalid Argument");
      }))
      .WillOnce(Invoke([&](std::string const& p) {
        ++count;
        EXPECT_EQ(2, count);
        EXPECT_EQ(payload_1, p);
        auto last_commited_byte = p.size() - 1;
        return make_status_or(ResumableUploadResponse{
            "", last_commited_byte, {}, ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t n) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_EQ(payload_2, p);
        EXPECT_EQ(payload_1.size() + payload_2.size(), n);
        auto last_committed_byte = payload_1.size() + payload_2.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Return(3 * quantum));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload_1.data(), payload_1.size());
  EXPECT_EQ(StatusCode::kInvalidArgument, streambuf.last_status().code())
      << ", status=" << streambuf.last_status();

  streambuf.sputn(payload_2.data(), payload_2.size());
  EXPECT_STATUS_OK(streambuf.last_status());

  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
