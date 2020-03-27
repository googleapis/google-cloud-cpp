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
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/internal/make_unique.h"
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
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SizeIs;

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
            "{}", 0, {}, ResumableUploadResponse::kInProgress, {}});
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
                                    ResumableUploadResponse::kInProgress,
                                    {}});
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

  auto quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '*');

  int count = 0;
  size_t next_byte = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    EXPECT_EQ(payload, p);
    auto last_committed_byte = payload.size() - 1;
    next_byte = last_committed_byte + 1;
    return make_status_or(ResumableUploadResponse{
        "", last_committed_byte, {}, ResumableUploadResponse::kInProgress, {}});
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
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));

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
  size_t next_byte = 0;
  {
    InSequence seq;
    EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(InvokeWithoutArgs([&]() {
      next_byte = payload_1.size();
      return make_status_or(
          ResumableUploadResponse{"",
                                  payload_1.size() - 1,
                                  {},
                                  ResumableUploadResponse::kInProgress,
                                  {}});
    }));
    EXPECT_CALL(
        *mock, UploadFinalChunk(payload_2, payload_1.size() + payload_2.size()))
        .WillOnce(Return(make_status_or(
            ResumableUploadResponse{"{}",
                                    payload_1.size() + payload_2.size() - 1,
                                    {},
                                    ResumableUploadResponse::kInProgress,
                                    {}})));
  }
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));

  ObjectWriteStreambuf streambuf(
      std::move(mock), 3 * quantum,
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
  size_t next_byte = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    auto expected = payload_1 + payload_2.substr(0, quantum - payload_1.size());
    EXPECT_EQ(expected, p);
    next_byte += p.size();
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
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
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));

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
  size_t next_byte = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    auto expected = payload;
    EXPECT_EQ(expected, p);
    next_byte += p.size();
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
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
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));
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

/// @test verify that bytes not accepted by GCS will be re-uploaded next Flush.
TEST(ObjectWriteStreambufTest, SomeBytesNotAccepted) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum, '*') + "abcde";

  size_t next_byte = 0;
  uint64_t const bytes_uploaded_first_try = quantum - 1;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    auto expected = payload.substr(0, quantum);
    EXPECT_EQ(expected, p);
    next_byte += bytes_uploaded_first_try;
    return make_status_or(
        ResumableUploadResponse{"",
                                bytes_uploaded_first_try - 1,
                                {},
                                ResumableUploadResponse::kInProgress,
                                {}});
  }));
  EXPECT_CALL(*mock, UploadFinalChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        EXPECT_EQ(p, payload.substr(bytes_uploaded_first_try));
        EXPECT_EQ(payload.size(), s);
        auto last_committed_byte = payload.size() - 1;
        return make_status_or(
            ResumableUploadResponse{"{}",
                                    last_committed_byte,
                                    {},
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  std::ostream output(&streambuf);
  output << payload;
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test verify that the upload steam transitions to a bad state if the next
/// expected byte jumps.
TEST(ObjectWriteStreambufTest, NextExpectedByteJumpsAhead) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum * 2, '*');

  size_t next_byte = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    next_byte += quantum * 2;
    auto expected = payload.substr(0, quantum);
    EXPECT_EQ(expected, p);
    return make_status_or(ResumableUploadResponse{
        "", next_byte - 1, {}, ResumableUploadResponse::kInProgress, {}});
  }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));
  std::string id = "id";
  EXPECT_CALL(*mock, session_id).WillOnce(ReturnRef(id));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());
  std::ostream output(&streambuf);
  output << payload;
  EXPECT_FALSE(output.good());
  EXPECT_EQ(streambuf.last_status().code(), StatusCode::kAborted);
}

/// @test verify that the upload steam transitions to a bad state if the next
/// expected byte decreases.
TEST(ObjectWriteStreambufTest, NextExpectedByteDecreases) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum * 2, '*');

  auto next_byte = quantum;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(InvokeWithoutArgs([&]() {
    next_byte--;
    return make_status_or(ResumableUploadResponse{
        "", next_byte - 1, {}, ResumableUploadResponse::kInProgress, {}});
  }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));
  EXPECT_CALL(*mock, done).WillRepeatedly(Return(false));
  std::string id = "id";
  EXPECT_CALL(*mock, session_id).WillOnce(ReturnRef(id));

  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());
  std::ostream output(&streambuf);
  output << payload;
  EXPECT_FALSE(output.good());
  EXPECT_EQ(streambuf.last_status().code(), StatusCode::kAborted);
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
  size_t next_byte = 0;
  EXPECT_CALL(*mock, UploadChunk(_)).WillOnce(Invoke([&](std::string const& p) {
    ++count;
    EXPECT_EQ(1, count);
    auto expected = payload_1 + payload_2.substr(0, quantum - payload_1.size());
    EXPECT_EQ(expected, p);
    next_byte += p.size();
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
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
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      }));
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Invoke([&]() {
    return next_byte;
  }));

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
  auto last_upload_response = make_status_or(ResumableUploadResponse{
      "url-for-test", 0, {}, ResumableUploadResponse::kDone, {}});
  EXPECT_CALL(*mock, last_response).WillOnce(ReturnRef(last_upload_response));

  ObjectWriteStreambuf streambuf(
      std::move(mock), UploadChunkRequest::kChunkSizeQuantum,
      google::cloud::internal::make_unique<NullHashValidator>());
  EXPECT_EQ(streambuf.IsOpen(), false);
  StatusOr<ResumableUploadResponse> close_result = streambuf.Close();
  ASSERT_STATUS_OK(close_result);
  EXPECT_EQ("url-for-test", close_result.value().upload_session_url);
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
  std::string const session_id = "upload_id";

  EXPECT_CALL(*mock, UploadChunk(SizeIs(quantum)))
      .WillOnce(
          Return(Status(StatusCode::kInvalidArgument, "Invalid Argument")));
  EXPECT_CALL(*mock, session_id).WillOnce(ReturnRef(session_id));
  ObjectWriteStreambuf streambuf(
      std::move(mock), quantum,
      google::cloud::internal::make_unique<NullHashValidator>());

  streambuf.sputn(payload_1.data(), payload_1.size());
  EXPECT_EQ(StatusCode::kInvalidArgument, streambuf.last_status().code())
      << ", status=" << streambuf.last_status();
  EXPECT_EQ(streambuf.resumable_session_id(), session_id);

  streambuf.sputn(payload_2.data(), payload_2.size());
  EXPECT_EQ(StatusCode::kInvalidArgument, streambuf.last_status().code())
      << ", status=" << streambuf.last_status();

  auto response = streambuf.Close();
  EXPECT_EQ(StatusCode::kInvalidArgument, response.status().code())
      << ", status=" << response.status();
}
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
