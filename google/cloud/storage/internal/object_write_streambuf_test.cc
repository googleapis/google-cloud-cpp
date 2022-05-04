// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/object_write_streambuf.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::InvokeWithoutArgs;

/// @test Verify that uploading an empty stream creates a single chunk.
TEST(ObjectWriteStreambufTest, EmptyStream) {
  auto mock = absl::make_unique<testing::MockClient>();

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_EQ(0, r.payload_size());
    EXPECT_EQ(0, r.offset());
    EXPECT_TRUE(r.last_chunk());
    return QueryResumableUploadResponse{absl::nullopt, ObjectMetadata()};
  });

  ObjectWriteStream stream(absl::make_unique<ObjectWriteStreambuf>(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/0,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled));
  stream.Close();
  EXPECT_STATUS_OK(stream.last_status());
}

/// @test Verify that streams auto-finalize if enabled.
TEST(ObjectWriteStreambufTest, AutoFinalizeEnabled) {
  auto mock = absl::make_unique<testing::MockClient>();

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_EQ(0, r.payload_size());
    EXPECT_EQ(0, r.offset());
    EXPECT_TRUE(r.last_chunk());
    return QueryResumableUploadResponse{absl::nullopt, ObjectMetadata()};
  });

  {
    ObjectWriteStream stream(absl::make_unique<ObjectWriteStreambuf>(
        std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
        /*committed_size=*/0, absl::nullopt,
        /*max_buffer_size=*/0, CreateNullHashFunction(), HashValues{},
        CreateNullHashValidator(), AutoFinalizeConfig::kEnabled));
  }
}

/// @test Verify that streams do not auto-finalize if so configured.
TEST(ObjectWriteStreambufTest, AutoFinalizeDisabled) {
  auto mock = absl::make_unique<testing::MockClient>();

  EXPECT_CALL(*mock, UploadChunk).Times(0);

  {
    ObjectWriteStream stream(absl::make_unique<ObjectWriteStreambuf>(
        std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
        /*committed_size=*/0, absl::nullopt,
        /*max_buffer_size=*/0, CreateNullHashFunction(), HashValues{},
        CreateNullHashValidator(), AutoFinalizeConfig::kDisabled));
  }
}

/// @test Verify that uploading a small stream creates a single chunk.
TEST(ObjectWriteStreambufTest, SmallStream) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = "small test payload";
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
    EXPECT_TRUE(r.last_chunk());
    EXPECT_EQ(r.upload_size().value_or(0), payload.size());
    return QueryResumableUploadResponse{absl::nullopt, ObjectMetadata()};
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(payload.data(), payload.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that uploading a stream which ends on an upload chunk quantum
/// works as expected.
TEST(ObjectWriteStreambufTest, EmptyTrailer) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '*');

  ::testing::InSequence sequence;
  std::uint64_t committed_size = 0;
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
        EXPECT_FALSE(r.last_chunk());
        committed_size += r.payload_size();
        return QueryResumableUploadResponse{committed_size, absl::nullopt};
      })
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{}));
        EXPECT_TRUE(r.last_chunk());
        committed_size += r.payload_size();
        return QueryResumableUploadResponse{committed_size, ObjectMetadata()};
      });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(payload.data(), payload.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream sends a single message for large payloads.
TEST(ObjectWriteStreambufTest, FlushAfterLargePayload) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const p0(3 * quantum, '*');
  std::string const p1("trailer");

  ::testing::InSequence seq;
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_FALSE(r.last_chunk());
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{p0}));
        return QueryResumableUploadResponse{p0.size(), absl::nullopt};
      })
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_TRUE(r.last_chunk());
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{p1}));
        return QueryResumableUploadResponse{r.offset() + r.payload_size(),
                                            ObjectMetadata()};
      });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/3 * quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(p0.data(), p0.size());
  streambuf.sputn(p1.data(), p1.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when a full quantum is available.
TEST(ObjectWriteStreambufTest, FlushAfterFullQuantum) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const p0("header");
  std::string const p1(quantum, '*');

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](UploadChunkRequest const& r) {
        auto const trailer = p1.substr(0, quantum - p0.size());
        EXPECT_THAT(r.payload(),
                    ElementsAre(ConstBuffer{p0}, ConstBuffer{trailer}));
        EXPECT_FALSE(r.last_chunk());
        return QueryResumableUploadResponse{r.offset() + r.payload_size(),
                                            absl::nullopt};
      })
      .WillOnce([&](UploadChunkRequest const& r) {
        auto const expected = p1.substr(p1.size() - p0.size());
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{expected}));
        EXPECT_TRUE(r.last_chunk());
        EXPECT_EQ(r.upload_size().value_or(0), p0.size() + p1.size());
        return QueryResumableUploadResponse{r.offset() + r.payload_size(),
                                            ObjectMetadata()};
      });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(p0.data(), p0.size());
  streambuf.sputn(p1.data(), p1.size());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when adding one character at a time.
TEST(ObjectWriteStreambufTest, OverflowFlushAtFullQuantum) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '*');

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_FALSE(r.last_chunk());
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
    return QueryResumableUploadResponse{quantum, absl::nullopt};
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    std::string expected = " ";
    EXPECT_TRUE(r.last_chunk());
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{expected}));
    return QueryResumableUploadResponse{quantum + 1, ObjectMetadata()};
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  for (auto const& c : payload) {
    EXPECT_EQ(c, streambuf.sputc(c));
  }
  EXPECT_EQ(' ', streambuf.sputc(' '));
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(ObjectWriteStreambuf::traits_type::eof(), streambuf.sputc(' '));
}

/// @test verify that bytes not accepted by GCS will be re-uploaded next Flush.
TEST(ObjectWriteStreambufTest, SomeBytesNotAccepted) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum - 2, '*') + "abcde";

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    auto expected = payload.substr(0, quantum);
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{expected}));
    return QueryResumableUploadResponse{quantum, absl::nullopt};
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    auto const expected = payload.substr(quantum);
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{expected}));
    return QueryResumableUploadResponse{quantum + expected.size(),
                                        ObjectMetadata()};
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  std::ostream output(&streambuf);
  output << payload;
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test verify that the upload stream transitions to a bad state if the
/// committed size jumps ahead.
TEST(ObjectWriteStreambufTest, CommittedSizeJumpsAhead) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum * 2, '*');

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    auto expected = payload.substr(0, 2 * quantum);
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{expected}));
    // Simulate a condition where the server reports more bytes committed
    // than expected
    return QueryResumableUploadResponse{3 * quantum, absl::nullopt};
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);
  std::ostream output(&streambuf);
  output << payload;
  EXPECT_FALSE(output.good());
  EXPECT_THAT(streambuf.last_status(), StatusIs(StatusCode::kAborted));
}

/// @test verify that the upload stream transitions to a bad state if the next
/// expected byte decreases.
TEST(ObjectWriteStreambufTest, CommittedSizeDecreases) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum * 2, '*');

  auto const initial_committed_size = 2 * quantum;
  EXPECT_CALL(*mock, UploadChunk).WillOnce(InvokeWithoutArgs([&]() {
    return QueryResumableUploadResponse{quantum, absl::nullopt};
  }));

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/initial_committed_size, absl::nullopt,
      /*max_buffer_size=*/quantum, CreateNullHashFunction(), HashValues{},
      CreateNullHashValidator(), AutoFinalizeConfig::kEnabled);
  std::ostream output(&streambuf);
  output << payload;
  EXPECT_FALSE(output.good());
  EXPECT_THAT(streambuf.last_status(), StatusIs(StatusCode::kAborted));
}

/// @test verify that the upload stream transitions to a bad state on a partial
/// write.
TEST(ObjectWriteStreambufTest, PartialUploadChunk) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = std::string(quantum * 4, '*');

  EXPECT_CALL(*mock, UploadChunk).WillOnce(InvokeWithoutArgs([&]() {
    return QueryResumableUploadResponse{2 * quantum, absl::nullopt};
  }));

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);
  std::ostream output(&streambuf);
  output.write(payload.data(), payload.size());
  EXPECT_FALSE(output.good());
  EXPECT_THAT(streambuf.last_status(), StatusIs(StatusCode::kAborted));
}

/// @test Verify that a stream flushes when mixing operations that add one
/// character at a time and operations that add buffers.
TEST(ObjectWriteStreambufTest, MixPutcPutn) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload_1("header");
  std::string const payload_2(quantum, '*');

  ::testing::InSequence sequence;
  std::uint64_t committed_size = 0;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_FALSE(r.last_chunk());
    auto expected = payload_2.substr(0, quantum - payload_1.size());
    EXPECT_THAT(r.payload(),
                ElementsAre(ConstBuffer{payload_1}, ConstBuffer{expected}));
    committed_size += r.payload_size();
    return QueryResumableUploadResponse{committed_size, absl::nullopt};
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_TRUE(r.last_chunk());
    auto expected = payload_2.substr(payload_2.size() - payload_1.size());
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{expected}));
    return QueryResumableUploadResponse{payload_1.size() + payload_2.size(),
                                        ObjectMetadata{}};
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

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
  auto mock = absl::make_unique<testing::MockClient>();

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id", 0,
      ObjectMetadata(),
      /*max_buffer_size=*/0, CreateNullHashFunction(), HashValues{},
      CreateNullHashValidator(), AutoFinalizeConfig::kEnabled);
  EXPECT_EQ(streambuf.IsOpen(), false);
  auto close_result = streambuf.Close();
  ASSERT_STATUS_OK(close_result);
}

/// @test A regression test for #8868.
///    https://github.com/googleapis/google-cloud-cpp/issues/8868
TEST(ObjectWriteStreambufTest, Regression8868) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');
  using ::testing::Return;

  ::testing::InSequence sequence;
  // Simulate an upload chunk that has some kind of transient error.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));
  // This should trigger a `QueryResumableUpload()`, simulate the case where
  // all the data is reported as "committed", but the payload is not reported
  // back.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{quantum, ObjectMetadata()}));

  using us = std::chrono::microseconds;
  auto retry = RetryClient::Create(
      std::move(mock),
      Options{}
          .set<Oauth2CredentialsOption>(oauth2::CreateAnonymousCredentials())
          .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(3).clone())
          .set<BackoffPolicyOption>(
              ExponentialBackoffPolicy(us(1), us(2), 2).clone())
          .set<IdempotencyPolicyOption>(
              AlwaysRetryIdempotencyPolicy{}.clone()));
  ObjectWriteStreambuf streambuf(
      std::move(retry), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0,
      /*metadata=*/absl::nullopt,
      /*max_buffer_size=*/2 * quantum, CreateNullHashFunction(), HashValues{},
      CreateNullHashValidator(), AutoFinalizeConfig::kEnabled);
  EXPECT_TRUE(streambuf.IsOpen());
  streambuf.sputn(payload.data(), payload.size());
  auto close = streambuf.Close();
  ASSERT_STATUS_OK(close);
  EXPECT_FALSE(streambuf.IsOpen());
  EXPECT_EQ(close->committed_size.value_or(0), quantum);
  EXPECT_TRUE(close->payload.has_value());

  close = streambuf.Close();
  ASSERT_STATUS_OK(close);
  EXPECT_FALSE(streambuf.IsOpen());
  EXPECT_EQ(close->committed_size.value_or(0), quantum);
  EXPECT_TRUE(close->payload.has_value());
}

/// @test Verify that last error status is accessible for small payload.
TEST(ObjectWriteStreambufTest, ErrorInSmallPayload) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload = "small test payload";

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
    return Status(StatusCode::kInvalidArgument, "Invalid Argument");
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(payload.data(), payload.size());
  auto response = streambuf.Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(streambuf.last_status(), StatusIs(StatusCode::kInvalidArgument));
}

/// @test Verify that last error status is accessible for large payloads.
TEST(ObjectWriteStreambufTest, ErrorInLargePayload) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload_1(3 * quantum, '*');
  std::string const payload_2("trailer");

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const&) {
    return Status(StatusCode::kInvalidArgument, "Invalid Argument");
  });
  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(payload_1.data(), payload_1.size());
  EXPECT_THAT(streambuf.last_status(), StatusIs(StatusCode::kInvalidArgument));
  EXPECT_EQ(streambuf.resumable_session_id(), "test-only-upload-id");

  streambuf.sputn(payload_2.data(), payload_2.size());
  EXPECT_THAT(streambuf.last_status(), StatusIs(StatusCode::kInvalidArgument));

  auto response = streambuf.Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));
}

/// @test Verify that uploads of known size work.
TEST(ObjectWriteStreambufTest, KnownSizeUpload) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(2 * quantum, '*');

  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
        return QueryResumableUploadResponse{2 * quantum, absl::nullopt};
      })
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
        return QueryResumableUploadResponse{4 * quantum, absl::nullopt};
      })
      .WillOnce([&](UploadChunkRequest const& r) {
        EXPECT_THAT(r.payload(),
                    ElementsAre(ConstBuffer{payload.data(), quantum}));
        // When using X-Upload-Content-Length GCS finalizes the upload when
        // enough data is sent, regardless of whether the client marks a chunk
        // as the final chunk. Furthermore, the response does not have a
        // committed size.
        return QueryResumableUploadResponse{absl::nullopt, ObjectMetadata()};
      });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  streambuf.sputn(payload.data(), 2 * quantum);
  streambuf.sputn(payload.data(), 2 * quantum);
  streambuf.sputn(payload.data(), quantum);
  EXPECT_EQ(5 * quantum, streambuf.next_expected_byte());
  EXPECT_FALSE(streambuf.IsOpen());
  EXPECT_STATUS_OK(streambuf.last_status());
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify flushing partially full buffers works.
TEST(ObjectWriteStreambufTest, Pubsync) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '*');

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
    EXPECT_FALSE(r.last_chunk());
    return QueryResumableUploadResponse{quantum, absl::nullopt};
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{payload}));
    EXPECT_TRUE(r.last_chunk());
    return QueryResumableUploadResponse{2 * quantum, ObjectMetadata()};
  });

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/2 * quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kEnabled);

  EXPECT_EQ(quantum, streambuf.sputn(payload.data(), quantum));
  EXPECT_EQ(0, streambuf.pubsync());
  EXPECT_EQ(quantum, streambuf.sputn(payload.data(), quantum));
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

/// @test Verify flushing too small a buffer does nothing.
TEST(ObjectWriteStreambufTest, PubsyncTooSmall) {
  auto mock = absl::make_unique<testing::MockClient>();

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const half = quantum / 2;
  std::string const p0(half, '0');

  // Write some data and flush it.  This should result in no calls to
  // UploadChunk(), as the buffer is too small to fill a chunk and "auto
  // finalize" is disabled.
  EXPECT_CALL(*mock, UploadChunk).Times(0);

  ObjectWriteStreambuf streambuf(
      std::move(mock), ResumableUploadRequest(), "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/2 * quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kDisabled);

  EXPECT_EQ(half, streambuf.sputn(p0.data(), half));
  EXPECT_EQ(0, streambuf.pubsync());
}

/// @test Verify custom headers are passed to UploadChunk()
TEST(ObjectWriteStreambufTest, WriteObjectWithCustomHeader) {
  auto mock = absl::make_unique<testing::MockClient>();
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto const p0 = std::string(quantum, '0');
  auto const p1 = std::string(quantum, '1');

  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](internal::UploadChunkRequest const& r) {
        EXPECT_FALSE(r.last_chunk());
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{p0}));
        EXPECT_EQ("header-value", r.GetOption<CustomHeader>().value_or(""));
        return internal::QueryResumableUploadResponse{r.payload_size(),
                                                      absl::nullopt};
      })
      .WillOnce([&](internal::UploadChunkRequest const& r) {
        EXPECT_FALSE(r.last_chunk());
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{p1}));
        EXPECT_EQ("header-value", r.GetOption<CustomHeader>().value_or(""));
        return internal::QueryResumableUploadResponse{
            r.offset() + r.payload_size(), absl::nullopt};
      })
      .WillOnce([&](internal::UploadChunkRequest const& r) {
        EXPECT_TRUE(r.last_chunk());
        EXPECT_THAT(r.payload(), ElementsAre(ConstBuffer{}));
        EXPECT_EQ("header-value", r.GetOption<CustomHeader>().value_or(""));
        return internal::QueryResumableUploadResponse{absl::nullopt,
                                                      ObjectMetadata()};
      });

  ObjectWriteStreambuf streambuf(
      std::move(mock),
      ResumableUploadRequest().set_option(
          CustomHeader("x-test-custom-header", "header-value")),
      "test-only-upload-id",
      /*committed_size=*/0, absl::nullopt, /*max_buffer_size=*/quantum,
      CreateNullHashFunction(), HashValues{}, CreateNullHashValidator(),
      AutoFinalizeConfig::kDisabled);

  EXPECT_EQ(p0.size(), streambuf.sputn(p0.data(), p0.size()));
  EXPECT_EQ(0, streambuf.pubsync());
  EXPECT_EQ(p1.size(), streambuf.sputn(p1.data(), p1.size()));
  auto response = streambuf.Close();
  EXPECT_STATUS_OK(response);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
