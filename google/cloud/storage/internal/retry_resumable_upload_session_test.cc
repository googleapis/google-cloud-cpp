// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/retry_resumable_upload_session.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::chrono_literals::operator"" _us;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;

class RetryResumableUploadSessionTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(1_us, 2_us, 2).clone();
}

/// @test Verify that transient failures are handled as expected.
TEST_F(RetryResumableUploadSessionTest, HandleTransient) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  // Verify that a transient on a UploadChunk() results in calls to
  // ResetSession() and that transients in these calls are retried too.
  ::testing::InSequence sequence_1;
  EXPECT_CALL(*mock, next_expected_byte).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession)
      .WillOnce(
          [&]() { return StatusOr<ResumableUploadResponse>(TransientError()); })
      .WillOnce([&]() {
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      });
  EXPECT_CALL(*mock, next_expected_byte).Times(1).WillRepeatedly(Return(0));

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(quantum));

  ::testing::InSequence sequence_2;

  // A simpler scenario where only the UploadChunk() calls fail.
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(2)
      .WillRepeatedly(Return(quantum));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(quantum));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return make_status_or(ResumableUploadResponse{
        "", 2 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(2 * quantum));

  // Even simpler scenario where only the UploadChunk() calls succeeds.
  ::testing::InSequence sequence_3;
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(2)
      .WillRepeatedly(Return(2 * quantum));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return make_status_or(ResumableUploadResponse{
        "", 3 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(3 * quantum));

  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response;
  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(quantum - 1, response->last_committed_byte);

  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(2 * quantum - 1, response->last_committed_byte);

  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(3 * quantum - 1, response->last_committed_byte);
}

/// @test Verify that a permanent error on UploadChunk results in a failure.
TEST_F(RetryResumableUploadSessionTest, PermanentErrorOnUpload) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(PermanentError());
  });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // The exact number transient errors tolerated by the policy is not relevant,
  // as the expectations will return a permanent error.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response = session.UploadChunk({{payload}});
  EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that a permanent error on ResetSession results in a failure.
TEST_F(RetryResumableUploadSessionTest, PermanentErrorOnReset) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });

  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return StatusOr<ResumableUploadResponse>(PermanentError());
  });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // The exact number transient errors tolerated by the policy is not relevant,
  // as the expectations will return a permanent error.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response = session.UploadChunk({{payload}});
  EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that ResetSession consumes the transient error budget.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnUploadChunk) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });

  // We only tolerate 2 transient errors, while the expectations are configured
  // to return 3 transients.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(2).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response = session.UploadChunk({{payload}});
  EXPECT_THAT(response, StatusIs(TransientError().code(),
                                 HasSubstr("Retry policy exhausted")));
}

/// @test Verify that too many transients on ResetSession result in a failure.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnReset) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });

  EXPECT_CALL(*mock, ResetSession).Times(2).WillRepeatedly([&]() {
    return StatusOr<ResumableUploadResponse>(TransientError());
  });

  // We only tolerate 2 transient errors, the third causes a permanent failure.
  // As described above, the first call to UploadChunk() will consume the full
  // budget.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(2).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response = session.UploadChunk({{payload}});
  EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that transients (or elapsed time) from different chunks do not
/// accumulate.
TEST_F(RetryResumableUploadSessionTest, HandleTransiensOnSeparateChunks) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  std::uint64_t next_expected_byte = 0;

  // In this test we do not care about how many times or when
  // next_expected_byte() is called, but it does need to return the right
  // values, the other mock functions set the correct return value using a local
  // variable.
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly([&]() {
    return next_expected_byte;
  });

  // There are a lot of calls expected. Basically we want to verify that the
  // transient error count is reset after each UploadChunk() succeeds, even
  // if counting all the transients across all the UploadChunk() calls exceeds
  // the retry limit.
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    next_expected_byte = quantum;
    return make_status_or(
        ResumableUploadResponse{"",
                                next_expected_byte - 1,
                                {},
                                ResumableUploadResponse::kInProgress,
                                {}});
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(
        ResumableUploadResponse{"",
                                next_expected_byte - 1,
                                {},
                                ResumableUploadResponse::kInProgress,
                                {}});
  });

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    next_expected_byte = 2 * quantum;
    return make_status_or(
        ResumableUploadResponse{"",
                                next_expected_byte - 1,
                                {},
                                ResumableUploadResponse::kInProgress,
                                {}});
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(
        ResumableUploadResponse{"",
                                next_expected_byte - 1,
                                {},
                                ResumableUploadResponse::kInProgress,
                                {}});
  });

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    next_expected_byte = 3 * quantum;
    return make_status_or(
        ResumableUploadResponse{"",
                                next_expected_byte - 1,
                                {},
                                ResumableUploadResponse::kInProgress,
                                {}});
  });

  // Configure a session that tolerates 2 transient errors per call. None of
  // the calls to UploadChunk() should use more than these.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(2).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(response->last_committed_byte, quantum - 1);

  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(response->last_committed_byte, 2 * quantum - 1);

  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(response->last_committed_byte, 3 * quantum - 1);
}

/// @test Verify that a permanent error on UploadFinalChunk results in a
/// failure.
TEST_F(RetryResumableUploadSessionTest, PermanentErrorOnUploadFinalChunk) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const&) {
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(PermanentError());
      });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // The retry policy settings are irrelevant, as the first permanent error
  // should break the retry loop.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  auto response = session.UploadFinalChunk({{payload}}, quantum, HashValues{});
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

/// @test Verify that too many transients on UploadFinalChunk result in a
/// failure.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnUploadFinalChunk) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const& h) {
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ("crc32c", h.crc32c);
        EXPECT_EQ("md5", h.md5);
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(TransientError());
      });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });

  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const& h) {
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ("crc32c", h.crc32c);
        EXPECT_EQ("md5", h.md5);
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(TransientError());
      });

  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const& h) {
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ("crc32c", h.crc32c);
        EXPECT_EQ("md5", h.md5);
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(TransientError());
      });

  // We only tolerate 3 transient errors, which will be consumed by the
  // failures in UploadFinalChunk.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(2).clone(),
                                      TestBackoffPolicy());

  auto response = session.UploadFinalChunk({{payload}}, quantum,
                                           HashValues{"crc32c", "md5"});
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(RetryResumableUploadSession, Done) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, done()).WillOnce(Return(true));

  RetryResumableUploadSession session(
      std::move(mock), LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone(),
      TestBackoffPolicy());
  EXPECT_TRUE(session.done());
}

TEST(RetryResumableUploadSession, LastResponse) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();
  StatusOr<ResumableUploadResponse> const last_response(ResumableUploadResponse{
      "url", 1, {}, ResumableUploadResponse::kDone, {}});
  EXPECT_CALL(*mock, last_response()).WillOnce(ReturnRef(last_response));

  RetryResumableUploadSession session(
      std::move(mock), LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone(),
      TestBackoffPolicy());
  StatusOr<ResumableUploadResponse> const& result = session.last_response();
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(result.value(), last_response.value());
}

TEST(RetryResumableUploadSession, UploadChunkPolicyExhaustedOnStart) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
  RetryResumableUploadSession session(
      std::move(mock), LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone(),
      TestBackoffPolicy());

  std::string data(UploadChunkRequest::kChunkSizeQuantum, 'X');
  auto res = session.UploadChunk({{data}});
  EXPECT_THAT(
      res, StatusIs(StatusCode::kDeadlineExceeded,
                    HasSubstr("Retry policy exhausted before first attempt")));
}

TEST(RetryResumableUploadSession, UploadFinalChunkPolicyExhaustedOnStart) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
  RetryResumableUploadSession session(
      std::move(mock), LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone(),
      TestBackoffPolicy());

  auto res =
      session.UploadFinalChunk({ConstBuffer{"blah", 4}}, 4, HashValues{});
  EXPECT_THAT(
      res, StatusIs(StatusCode::kDeadlineExceeded,
                    HasSubstr("Retry policy exhausted before first attempt")));
}

TEST(RetryResumableUploadSession, ResetSessionPolicyExhaustedOnStart) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();
  RetryResumableUploadSession session(
      std::move(mock), LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone(),
      TestBackoffPolicy());
  auto res = session.ResetSession();
  EXPECT_THAT(
      res, StatusIs(StatusCode::kDeadlineExceeded,
                    HasSubstr("Retry policy exhausted before first attempt")));
}

/// @test Verify that transient failures which move next_bytes are handled
TEST_F(RetryResumableUploadSessionTest, HandleTransientPartialFailures) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum, 'X') +
                            std::string(quantum, 'Y') +
                            std::string(quantum, 'Z'));

  std::string const payload_final(std::string(quantum, 'A') +
                                  std::string(quantum, 'B') +
                                  std::string(quantum, 'C'));

  ::testing::InSequence sequence;
  // First we create a few series of calls to UploadChunk(), the first two
  // fail with a transient error. When calling ResetSession() we discover that
  // they have been partially successful.
  EXPECT_CALL(*mock, next_expected_byte).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_EQ(3 * quantum, TotalBytes(p));
    EXPECT_EQ('X', p[0][0]);
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(quantum));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_EQ(2 * quantum, TotalBytes(p));
    EXPECT_EQ('Y', p[0][0]);
    return StatusOr<ResumableUploadResponse>(TransientError());
  });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 2 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(2 * quantum));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_EQ(quantum, TotalBytes(p));
    EXPECT_EQ('Z', p[0][0]);
    return make_status_or(ResumableUploadResponse{
        "", 3 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(3)
      .WillRepeatedly(Return(3 * quantum));

  // Next we do something similar with UploadFinalChunk.
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce(
          [&](ConstBufferSequence const& p, std::uint64_t, HashValues const&) {
            EXPECT_EQ(3 * quantum, TotalBytes(p));
            EXPECT_EQ('A', p[0][0]);
            return StatusOr<ResumableUploadResponse>(TransientError());
          });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 4 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(4 * quantum));
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce(
          [&](ConstBufferSequence const& p, std::uint64_t, HashValues const&) {
            EXPECT_EQ(2 * quantum, TotalBytes(p));
            EXPECT_EQ('B', p[0][0]);
            return StatusOr<ResumableUploadResponse>(TransientError());
          });
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 5 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte)
      .Times(1)
      .WillRepeatedly(Return(5 * quantum));
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce(
          [&](ConstBufferSequence const& p, std::uint64_t, HashValues const&) {
            EXPECT_EQ(quantum, TotalBytes(p));
            EXPECT_EQ('C', p[0][0]);
            return make_status_or(ResumableUploadResponse{
                "", 6 * quantum - 1, {}, ResumableUploadResponse::kDone, {}});
          });

  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response;
  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(3 * quantum - 1, response->last_committed_byte);

  response =
      session.UploadFinalChunk({{payload_final}}, 6 * quantum, HashValues{});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(6 * quantum - 1, response->last_committed_byte);
}

/// @test Verify that erroneous server behavior (uncommitting data) is handled.
TEST_F(RetryResumableUploadSessionTest, UploadFinalChunkUncommitted) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum, 'X'));

  auto const session_id_value = std::string{"test-only-session-id"};
  EXPECT_CALL(*mock, session_id).WillRepeatedly(ReturnRef(session_id_value));

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, next_expected_byte()).Times(2).WillRepeatedly(Return(0));
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const&) {
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte())
      .Times(3)
      .WillRepeatedly(Return(quantum));
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce(
          [&](ConstBufferSequence const&, std::uint64_t, HashValues const&) {
            return StatusOr<ResumableUploadResponse>(TransientError());
          });

  // This should not happen, the last committed byte should not go back
  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, next_expected_byte()).Times(1).WillRepeatedly(Return(0));

  StatusOr<ResumableUploadResponse> response;
  EXPECT_CALL(*mock, last_response())
      .WillOnce([&response]() -> StatusOr<ResumableUploadResponse> const& {
        return response;
      });

  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  response = session.UploadChunk({{payload}});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(quantum - 1, response->last_committed_byte);

  response = session.UploadFinalChunk({{payload}}, 2 * quantum, HashValues{});
  ASSERT_FALSE(response);
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("https://github.com/")));
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal,
                                 HasSubstr("google-cloud-cpp/issues/new")));
}

/// @test Verify that retry exhaustion following a short write fails.
TEST_F(RetryResumableUploadSessionTest, ShortWriteRetryExhausted) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum * 2, 'X'));

  std::uint64_t neb = 0;
  EXPECT_CALL(*mock, next_expected_byte).WillRepeatedly([&] { return neb; });

  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const&) {
        return make_status_or(ResumableUploadResponse{
            "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        EXPECT_EQ(TotalBytes(p), payload.size() - neb);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        EXPECT_EQ(TotalBytes(p), payload.size() - neb);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        EXPECT_EQ(TotalBytes(p), payload.size() - neb);
        return StatusOr<ResumableUploadResponse>(TransientError());
      });

  EXPECT_CALL(*mock, ResetSession()).WillRepeatedly([&]() {
    return make_status_or(ResumableUploadResponse{
        "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });

  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(2).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response;
  response = session.UploadChunk({{payload}});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that short writes are retried.
TEST_F(RetryResumableUploadSessionTest, ShortWriteRetrySucceeds) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum * 2, 'X'));

  std::uint64_t neb = 0;
  EXPECT_CALL(*mock, next_expected_byte).WillRepeatedly([&] { return neb; });

  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const&) {
        neb = quantum;
        return make_status_or(ResumableUploadResponse{
            "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&](ConstBufferSequence const&) {
        neb = 2 * quantum;
        return make_status_or(ResumableUploadResponse{
            "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
      });

  EXPECT_CALL(*mock, ResetSession()).WillRepeatedly([&]() {
    return make_status_or(ResumableUploadResponse{
        "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });

  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response;
  response = session.UploadChunk({{payload}});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(2 * quantum - 1, response->last_committed_byte);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
