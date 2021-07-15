// Copyright 2018 Google LLC
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
inline namespace STORAGE_CLIENT_NS {
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // RetryResumableUploadSession::UploadChunk() is called
  // 1. next_expected_byte() -> returns 0
  // 2. next_expected_byte() -> returns 0
  // 3. UploadChunk() -> returns transient error
  // 4. ResetSession() -> returns transient error
  // 5. ResetSession() -> returns success (0 bytes committed)
  // 6  next_expected_byte() -> returns 0
  // 7. UploadChunk() -> returns success (quantum bytes committed)
  // 8. next_expected_byte() -> returns quantum
  // RetryResumableUploadSession::UploadChunk() is called
  // 9. next_expected_byte() -> returns quantum
  // 10. next_expected_byte() -> returns quantum
  // 11. UploadChunk() -> returns transient error
  // 12. ResetSession() -> returns success (quantum bytes committed)
  // 13. next_expected_byte() -> returns quantum
  // 14. UploadChunk() -> returns success (2 * quantum bytes committed)
  // 15. next_expected_byte() -> returns 2 * quantum
  // RetryResumableUploadSession::UploadChunk() is called
  // 16. next_expected_byte() -> returns 2 * quantum
  // 17. next_expected_byte() -> returns 2 * quantum
  // 18. UploadChunk() -> returns success (3 * quantum bytes committed)
  // 18. next_expected_byte() -> returns 3 * quantum
  //
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(7, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return make_status_or(ResumableUploadResponse{
            "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(11, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(14, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return make_status_or(ResumableUploadResponse{
            "", 2 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(18, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return make_status_or(ResumableUploadResponse{
            "", 3 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      });

  EXPECT_CALL(*mock, ResetSession())
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(4, count);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(5, count);
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(12, count);
        return make_status_or(ResumableUploadResponse{
            "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      });

  EXPECT_CALL(*mock, next_expected_byte())
      .WillOnce([&]() {
        EXPECT_EQ(1, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(2, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(6, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(8, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(9, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(10, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(13, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(15, ++count);
        return 2 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(16, ++count);
        return 2 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(17, ++count);
        return 2 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(19, ++count);
        return 3 * quantum;
      });

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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // Ignore next_expected_byte() in tests - it will always return 0.
  // 1. UploadChunk() -> returns permanent error, the request aborts.
  //
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    ++count;
    EXPECT_EQ(1, count);
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(PermanentError());
  });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // We only tolerate 4 transient errors, the first call to UploadChunk() will
  // consume the full budget.
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // Ignore next_expected_byte() in tests - it will always return 0.
  // 1. UploadChunk() -> returns transient error
  // 2. ResetSession() -> returns permanent, the request aborts.
  //
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    ++count;
    EXPECT_EQ(1, count);
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });

  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    ++count;
    EXPECT_EQ(2, count);
    return StatusOr<ResumableUploadResponse>(PermanentError());
  });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // We only tolerate 4 transient errors, the first call to UploadChunk() will
  // consume the full budget.
  RetryResumableUploadSession session(std::move(mock),
                                      LimitedErrorCountRetryPolicy(10).clone(),
                                      TestBackoffPolicy());

  StatusOr<ResumableUploadResponse> response = session.UploadChunk({{payload}});
  EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that too many transients on ResetSession results in a failure.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnUploadChunk) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // Ignore next_expected_byte() in tests - it will always return 0.
  // 1. UploadChunk() -> returns transient error
  // 2. ResetSession() -> returns success (0 bytes committed)
  // 3. UploadChunk() -> returns transient error
  // 4. ResetSession() -> returns success (0 bytes committed)
  // 5. UploadChunk() -> returns transient error, the policy is exhausted.
  //
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(5, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      });

  EXPECT_CALL(*mock, ResetSession())
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(2, count);
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(4, count);
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // We only tolerate 4 transient errors, the first call to UploadChunk() will
  // consume the full budget.
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // RetryResumableUploadSession::UploadChunk() is called
  // 1. next_expected_byte() -> returns 0
  // 2. next_expected_byte() -> returns 0
  // 3. UploadChunk() -> returns transient error
  // 4. ResetSession() -> returns transient error
  // 5. ResetSession() -> returns transient error, the policy is exhausted
  //
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    ++count;
    EXPECT_EQ(3, count);
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(TransientError());
  });

  EXPECT_CALL(*mock, ResetSession())
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(4, count);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(5, count);
        return StatusOr<ResumableUploadResponse>(TransientError());
      });

  EXPECT_CALL(*mock, next_expected_byte())
      .WillOnce([&]() {
        EXPECT_EQ(1, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(2, ++count);
        return 0;
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

  // Keep track of the sequence of calls.
  int count = 0;
  std::uint64_t next_expected_byte = 0;

  // In this test we do not care about how many times or when next
  // expected_byte() is called, but it does need to return the right values,
  // the other mock functions set the correct return value using a local
  // variable.
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly([&]() {
    return next_expected_byte;
  });

  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // Ignore next_expected_byte() in tests - it will always return 0.
  // 1. UploadChunk() -> returns transient error
  // 2. ResetSession() -> returns success (0 bytes committed)
  // 3. UploadChunk() -> returns success
  // 4. UploadChunk() -> returns transient error
  // 5. ResetSession() -> returns success (0 bytes committed)
  // 6. UploadChunk() -> returns success
  // 7. UploadChunk() -> returns transient error
  // 8. ResetSession() -> returns success (0 bytes committed)
  // 9. UploadChunk() -> returns success
  //
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        next_expected_byte = quantum;
        return make_status_or(
            ResumableUploadResponse{"",
                                    next_expected_byte - 1,
                                    {},
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(4, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(6, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        next_expected_byte = 2 * quantum;
        return make_status_or(
            ResumableUploadResponse{"",
                                    next_expected_byte - 1,
                                    {},
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(7, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(9, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        next_expected_byte = 3 * quantum;
        return make_status_or(
            ResumableUploadResponse{"",
                                    next_expected_byte - 1,
                                    {},
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      });

  EXPECT_CALL(*mock, ResetSession())
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(2, count);
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(5, count);
        return make_status_or(
            ResumableUploadResponse{"",
                                    next_expected_byte - 1,
                                    {},
                                    ResumableUploadResponse::kInProgress,
                                    {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(8, count);
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // Ignore next_expected_byte() in tests - it will always return 0.
  // 1. UploadChunk() -> returns permanent error, the request aborts.
  //
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const&) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(PermanentError());
      });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // We only tolerate 4 transient errors, the first call to UploadChunk() will
  // consume the full budget.
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // Ignore next_expected_byte() in tests - it will always return 0.
  // 1. UploadFinalChunk() -> returns transient error
  // 2. ResetSession() -> returns success (0 bytes committed)
  // 3. UploadFinalChunk() -> returns transient error
  // 4. ResetSession() -> returns success (0 bytes committed)
  // 5. UploadFinalChunk() -> returns transient error, the policy is exhausted.
  //
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const& h) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ("crc32c", h.crc32c);
        EXPECT_EQ("md5", h.md5);
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const& h) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ("crc32c", h.crc32c);
        EXPECT_EQ("md5", h.md5);
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const& h) {
        ++count;
        EXPECT_EQ(5, count);
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ("crc32c", h.crc32c);
        EXPECT_EQ("md5", h.md5);
        EXPECT_EQ(quantum, s);
        return StatusOr<ResumableUploadResponse>(TransientError());
      });

  EXPECT_CALL(*mock, ResetSession())
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(2, count);
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(4, count);
        return make_status_or(ResumableUploadResponse{
            "", 0, {}, ResumableUploadResponse::kInProgress, {}});
      });

  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));

  // We only tolerate 4 transient errors, the first call to UploadChunk() will
  // consume the full budget.
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // 1. next_expected_byte() -> returns 0
  // 2. next_expected_byte() -> returns 0
  // 3. UploadChunk() -> returns transient error
  // 4. ResetSession() -> returns success (quantum bytes committed)
  // 5. next_expected_byte() -> returns quantum
  // 6. UploadChunk() -> returns transient error
  // 7. ResetSession() -> returns success (2 * quantum bytes committed)
  // 8. next_expected_byte() -> returns 2 * quantum
  // 9. UploadChunk() -> returns success (3 * quantum bytes committed)
  // 10. next_expected_byte() -> returns 3 * quantum
  //
  // 11. next_expected_byte() -> returns 3 * quantum
  // 12. next_expected_byte() -> returns 3 * quantum
  // 13. UploadFinalChunk() -> returns transient error
  // 14. ResetSession() -> returns success (4 * quantum bytes committed)
  // 15. next_expected_byte() -> returns 4 * quantum
  // 16. UploadFinalChunk() -> returns transient error
  // 17. ResetSession() -> returns success (5 * quantum bytes committed)
  // 18. next_expected_byte() -> returns 5 * quantum
  // 19. UploadFinalChunk() -> returns success (6 * quantum bytes committed)
  // 20. next_expected_byte() -> returns 6 * quantum
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_EQ(3 * quantum, TotalBytes(p));
        EXPECT_EQ('X', p[0][0]);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(6, count);
        EXPECT_EQ(2 * quantum, TotalBytes(p));
        EXPECT_EQ('Y', p[0][0]);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(9, count);
        EXPECT_EQ(quantum, TotalBytes(p));
        EXPECT_EQ('Z', p[0][0]);
        return make_status_or(ResumableUploadResponse{
            "", 3 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      });
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce(
          [&](ConstBufferSequence const& p, std::uint64_t, HashValues const&) {
            ++count;
            EXPECT_EQ(13, count);
            EXPECT_EQ(3 * quantum, TotalBytes(p));
            EXPECT_EQ('A', p[0][0]);
            return StatusOr<ResumableUploadResponse>(TransientError());
          })
      .WillOnce(
          [&](ConstBufferSequence const& p, std::uint64_t, HashValues const&) {
            ++count;
            EXPECT_EQ(16, count);
            EXPECT_EQ(2 * quantum, TotalBytes(p));
            EXPECT_EQ('B', p[0][0]);
            return StatusOr<ResumableUploadResponse>(TransientError());
          })
      .WillOnce(
          [&](ConstBufferSequence const& p, std::uint64_t, HashValues const&) {
            ++count;
            EXPECT_EQ(19, count);
            EXPECT_EQ(quantum, TotalBytes(p));
            EXPECT_EQ('C', p[0][0]);
            return make_status_or(ResumableUploadResponse{
                "", 6 * quantum - 1, {}, ResumableUploadResponse::kDone, {}});
          });

  EXPECT_CALL(*mock, ResetSession())
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(4, count);
        return make_status_or(ResumableUploadResponse{
            "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(7, count);
        return make_status_or(ResumableUploadResponse{
            "", 2 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(14, count);
        return make_status_or(ResumableUploadResponse{
            "", 4 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&]() {
        ++count;
        EXPECT_EQ(17, count);
        return make_status_or(ResumableUploadResponse{
            "", 5 * quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
      });

  EXPECT_CALL(*mock, next_expected_byte())
      .WillOnce([&]() {
        EXPECT_EQ(1, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(2, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(5, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(8, ++count);
        return 2 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(10, ++count);
        return 3 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(11, ++count);
        return 3 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(12, ++count);
        return 3 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(15, ++count);
        return 4 * quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(18, ++count);
        return 5 * quantum;
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

  // Keep track of the sequence of calls.
  int count = 0;
  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // 1. next_expected_byte() -> returns 0
  // 2. next_expected_byte() -> returns 0
  // 3. UploadChunk() -> returns success (quantum bytes committed)
  // 4. next_expected_byte() -> returns quantum
  //
  // 5. next_expected_byte() -> returns quantum
  // 6. next_expected_byte() -> returns quantum
  // 7. UploadFinalChunk() -> returns transient error
  // 8. ResetSession() -> returns success (0 bytes committed)
  // 9. next_expected_byte() -> returns 0
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const&) {
    ++count;
    EXPECT_EQ(3, count);
    return make_status_or(ResumableUploadResponse{
        "", quantum - 1, {}, ResumableUploadResponse::kInProgress, {}});
  });
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce(
          [&](ConstBufferSequence const&, std::uint64_t, HashValues const&) {
            ++count;
            EXPECT_EQ(7, count);
            return StatusOr<ResumableUploadResponse>(TransientError());
          });

  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    ++count;
    EXPECT_EQ(8, count);
    return make_status_or(ResumableUploadResponse{
        "", 0, {}, ResumableUploadResponse::kInProgress, {}});
  });

  EXPECT_CALL(*mock, next_expected_byte())
      .WillOnce([&]() {
        EXPECT_EQ(1, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(2, ++count);
        return 0;
      })
      .WillOnce([&]() {
        EXPECT_EQ(4, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(5, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(6, ++count);
        return quantum;
      })
      .WillOnce([&]() {
        EXPECT_EQ(9, ++count);
        return 0;
      });

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
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal, HasSubstr("github")));
}

/// @test Verify that retry exhaustion following a short write fails.
TEST_F(RetryResumableUploadSessionTest, ShortWriteRetryExhausted) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum * 2, 'X'));

  // Keep track of the sequence of calls.
  int count = 0;
  std::uint64_t neb = 0;
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly([&]() {
    return neb;
  });

  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // 1. UploadChunk() -> success (quantum committed instead of 2*quantum)
  // 2. UploadChunk() -> transient
  // 3. Retry policy is exhausted.
  //
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const&) {
        ++count;
        EXPECT_EQ(1, count);
        return make_status_or(ResumableUploadResponse{
            "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(2, count);
        EXPECT_EQ(TotalBytes(p), payload.size() - neb);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_EQ(TotalBytes(p), payload.size() - neb);
        return StatusOr<ResumableUploadResponse>(TransientError());
      })
      .WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(4, count);
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

  // Keep track of the sequence of calls.
  int count = 0;
  std::uint64_t neb = 0;
  EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly([&]() {
    return neb;
  });

  // The sequence of messages is split across two EXPECT_CALL() and hard to see,
  // basically we want this to happen:
  //
  // 1. UploadChunk() -> success (quantum committed instead of 2*quantum)
  // 2. UploadChunk() -> success (2* quantum committed)
  //
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](ConstBufferSequence const&) {
        ++count;
        EXPECT_EQ(1, count);
        neb = quantum;
        return make_status_or(ResumableUploadResponse{
            "", neb - 1, {}, ResumableUploadResponse::kInProgress, {}});
      })
      .WillOnce([&](ConstBufferSequence const&) {
        ++count;
        EXPECT_EQ(2, count);
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
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
