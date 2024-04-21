// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/crc32c.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_hash_function.h"
#include "google/cloud/storage/testing/mock_resume_policy.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockAsyncObjectMediaStream;
using ::google::cloud::storage::testing::MockHashFunction;
using ::google::cloud::storage::testing::MockResumePolicy;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::protobuf::TextFormat;
using ::testing::ElementsAre;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::VariantWith;

using AsyncReadObjectStream = ::google::cloud::internal::AsyncStreamingReadRpc<
    google::storage::v2::ReadObjectResponse>;

class AsyncConnectionImplTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kAuthority = "storage.googleapis.com";

auto TestOptions(Options options = {}) {
  using ms = std::chrono::milliseconds;
  options = internal::MergeOptions(
      std::move(options),
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(2).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone()));
  return DefaultOptionsAsync(std::move(options));
}

std::shared_ptr<storage_experimental::AsyncConnection> MakeTestConnection(
    CompletionQueue cq, std::shared_ptr<storage::testing::MockStorageStub> mock,
    Options options = {}) {
  return MakeAsyncConnection(std::move(cq), std::move(mock),
                             TestOptions(std::move(options)));
}

std::unique_ptr<AsyncReadObjectStream> MakeErrorReadStream(
    AsyncSequencer<bool>& sequencer, Status const& status) {
  auto stream = std::make_unique<MockAsyncObjectMediaStream>();
  EXPECT_CALL(*stream, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&, status] {
    return sequencer.PushBack("Finish").then([status](auto) { return status; });
  });
  return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
}

TEST_F(AsyncConnectionImplTest, ReadObject) {
  auto constexpr kExpectedRequest = R"pb(
    bucket: "test-only-invalid"
    object: "test-object"
    generation: 24
    if_generation_match: 42
  )pb";
  AsyncSequencer<bool> sequencer;
  auto make_success_stream = [&](AsyncSequencer<bool>& sequencer) {
    auto stream = std::make_unique<MockAsyncObjectMediaStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Read)
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            google::storage::v2::ReadObjectResponse response;
            response.mutable_metadata()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_metadata()->set_name("test-object");
            response.mutable_metadata()->set_size(4096);
            response.mutable_content_range()->set_start(1024);
            response.mutable_content_range()->set_end(2048);
            return absl::make_optional(response);
          });
        })
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            return absl::optional<google::storage::v2::ReadObjectResponse>();
          });
        });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
  };

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce(
          [&] { return MakeErrorReadStream(sequencer, TransientError()); })
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const& options,
                    google::storage::v2::ReadObjectRequest const& request) {
        // Verify at least one option is initialized with the correct value.
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);
        auto expected = google::storage::v2::ReadObjectRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return make_success_stream(sequencer);
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillRepeatedly([&sequencer](auto d) {
    auto deadline =
        std::chrono::system_clock::now() +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(d);
    return sequencer.PushBack("MakeRelativeTimer").then([deadline](auto f) {
      if (f.get()) return make_status_or(deadline);
      return StatusOr<std::chrono::system_clock::time_point>(
          Status(StatusCode::kCancelled, "cancelled"));
    });
  });

  auto connection =
      MakeTestConnection(CompletionQueue(mock_cq), mock,
                         Options{}.set<storage::DownloadStallTimeoutOption>(
                             std::chrono::seconds(0)));
  auto request = google::storage::v2::ReadObjectRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &request));
  auto pending =
      connection->ReadObject({std::move(request), connection->options()});

  ASSERT_TRUE(pending.is_ready());
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);
  auto data = reader->Read();

  // First simulate a failed `ReadObject()`. This returns a streaming RPC
  // that completes with `false` on `Start()` (i.e. never starts) and then
  // completes with a transient error on `Finish()`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  // The retry loop will set a timer to backoff.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "MakeRelativeTimer");
  next.first.set_value(true);

  // Then simulate a successful `ReadObject()`. This returns a streaming
  // RPC that completes with `true` on `Start()`, then returns some data on the
  // first `Read()`, then an unset optional on the second `Read()` (indicating
  // 'end of the streaming RPC'), and then a success `Status` for `Finish()`.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  auto response = data.get();
  ASSERT_TRUE(
      absl::holds_alternative<storage_experimental::ReadPayload>(response));

  // The `Read()` and `Finish()` calls must happen before the second `Read()` is
  // satisfied.
  data = reader->Read();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  EXPECT_THAT(data.get(), VariantWith<Status>(IsOk()));
}

TEST_F(AsyncConnectionImplTest, ReadObjectWithTimeout) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    auto stream = std::make_unique<MockAsyncObjectMediaStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Read)
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            google::storage::v2::ReadObjectResponse response;
            response.mutable_metadata()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_metadata()->set_name("test-object");
            response.mutable_metadata()->set_size(4096);
            response.mutable_content_range()->set_start(1024);
            response.mutable_content_range()->set_end(2048);
            return absl::make_optional(response);
          });
        })
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            return absl::optional<google::storage::v2::ReadObjectResponse>();
          });
        });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
  });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  // We will configure the connection to use 1 second timeouts.
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(
                            std::chrono::nanoseconds(std::chrono::seconds(1))))
      .WillRepeatedly([&sequencer](auto d) {
        auto deadline =
            std::chrono::system_clock::now() +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(d);
        return sequencer.PushBack("MakeRelativeTimer").then([deadline](auto f) {
          if (f.get()) return make_status_or(deadline);
          return StatusOr<std::chrono::system_clock::time_point>(
              Status(StatusCode::kCancelled, "cancelled"));
        });
      });

  auto connection = MakeTestConnection(
      CompletionQueue(mock_cq), mock,
      Options{}
          .set<storage::DownloadStallTimeoutOption>(std::chrono::seconds(1))
          .set<storage::DownloadStallMinimumRateOption>(2 * 1024 * 1024L));
  auto pending = connection->ReadObject(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});

  ASSERT_TRUE(pending.is_ready());
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);

  // Start a read.
  auto data = reader->Read();
  // Because the timeout parameters are configured, the first thing to happen is
  // that a timer is set.
  auto timer = sequencer.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  // Then the `Start()` operation is scheduled.  Either that complete first (and
  // then cancels the timer) or the timer completes first (and cancels the
  // streaming RPC).
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  timer.first.set_value(false);  // simulate a cancelled timer.
  next.first.set_value(true);

  // Then the `Read()` operation and its timer are scheduled:
  timer = sequencer.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  timer.first.set_value(false);  // simulate a cancelled timer.
  next.first.set_value(true);

  auto response = data.get();
  ASSERT_TRUE(
      absl::holds_alternative<storage_experimental::ReadPayload>(response));

  // Trigger another read. Since this closes the stream, the `Read()` and
  // `Finish()` calls must happen before the second `Read()` is satisfied.
  data = reader->Read();
  timer = sequencer.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  timer.first.set_value(false);  // simulate a canceled timeout
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  EXPECT_THAT(data.get(), VariantWith<Status>(IsOk()));
}

TEST_F(AsyncConnectionImplTest, ReadObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    return MakeErrorReadStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ReadObject(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});
  ASSERT_TRUE(pending.is_ready());
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);
  auto data = reader->Read();

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = data.get();
  EXPECT_THAT(response, VariantWith<Status>(StatusIs(PermanentError().code())));
}

TEST_F(AsyncConnectionImplTest, ReadObjectTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).Times(3).WillRepeatedly([&] {
    return MakeErrorReadStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ReadObject(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);
  auto data = reader->Read();

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value(false);

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value(true);
  }

  auto response = data.get();
  EXPECT_THAT(response, VariantWith<Status>(StatusIs(TransientError().code())));
}

// Only one test for ReadObjectRange(). The tests for
// `AsyncAccumulateReadObjectFull()` cover most other cases.
TEST_F(AsyncConnectionImplTest, ReadObjectRangePermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    return MakeErrorReadStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ReadObjectRange(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});
  ASSERT_FALSE(pending.is_ready());
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  EXPECT_THAT(pending.get(), StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, ReadObjectDetectBadMessageChecksum) {
  AsyncSequencer<bool> sequencer;
  auto make_bad_checksum_stream = [&](AsyncSequencer<bool>& sequencer) {
    auto stream = std::make_unique<MockAsyncObjectMediaStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        google::storage::v2::ReadObjectResponse response;
        response.mutable_metadata()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_metadata()->set_name("test-object");
        response.mutable_metadata()->set_generation(12345);
        auto constexpr kQuick = "The quick brown fox jumps over the lazy dog";
        SetMutableContent(*response.mutable_checksummed_data(),
                          ContentType(kQuick));
        // Deliberatively set the checksum to an incorrect value.
        response.mutable_checksummed_data()->set_crc32c(Crc32c(kQuick) + 1);
        return absl::make_optional(response);
      });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
  };

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    return make_bad_checksum_stream(sequencer);
  });

  auto mock_resume_policy_factory =
      []() -> std::unique_ptr<storage_experimental::ResumePolicy> {
    auto policy = std::make_unique<MockResumePolicy>();
    EXPECT_CALL(*policy, OnStartSuccess).Times(1);
    EXPECT_CALL(*policy, OnFinish(StatusIs(StatusCode::kInvalidArgument)))
        .WillOnce(Return(storage_experimental::ResumePolicy::kStop));
    return policy;
  };

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(
      pool.cq(), mock,
      Options{}.set<storage_experimental::ResumePolicyOption>(
          mock_resume_policy_factory));
  auto pending = connection->ReadObject(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});

  ASSERT_TRUE(pending.is_ready());
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);
  auto data = reader->Read();

  // This stream starts successfully:
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  // However, the `Read()` call returns an error because the checksum is invalid
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  auto response = data.get();
  EXPECT_THAT(response,
              VariantWith<Status>(StatusIs(StatusCode::kInvalidArgument)));

  // The stream Finish() function should be called in the background.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, ReadObjectDetectBadFullChecksum) {
  auto constexpr kQuick = "The quick brown fox jumps over the lazy dog";
  AsyncSequencer<bool> sequencer;
  auto make_bad_checksum_stream = [&](AsyncSequencer<bool>& sequencer) {
    auto stream = std::make_unique<MockAsyncObjectMediaStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Read)
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([&](auto) {
            google::storage::v2::ReadObjectResponse response;
            response.mutable_metadata()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_metadata()->set_name("test-object");
            response.mutable_metadata()->set_generation(12345);
            SetMutableContent(*response.mutable_checksummed_data(),
                              ContentType(kQuick));
            response.mutable_checksummed_data()->set_crc32c(Crc32c(kQuick));
            // Set the full object checksums to an intentionally incorrect
            // value. The client should detect this error and report it at the
            // end of the download.
            auto full_crc32c = std::uint32_t{0};
            full_crc32c = ExtendCrc32c(full_crc32c, kQuick);
            full_crc32c = ExtendCrc32c(full_crc32c, kQuick);
            response.mutable_object_checksums()->set_crc32c(full_crc32c + 1);
            return absl::make_optional(response);
          });
        })
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([&](auto) {
            google::storage::v2::ReadObjectResponse response;
            SetMutableContent(*response.mutable_checksummed_data(),
                              ContentType(kQuick));
            response.mutable_checksummed_data()->set_crc32c(Crc32c(kQuick));
            return absl::make_optional(response);
          });
        })
        .WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            return absl::optional<google::storage::v2::ReadObjectResponse>{};
          });
        });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
  };

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    return make_bad_checksum_stream(sequencer);
  });

  auto mock_resume_policy_factory =
      []() -> std::unique_ptr<storage_experimental::ResumePolicy> {
    auto policy = std::make_unique<MockResumePolicy>();
    EXPECT_CALL(*policy, OnStartSuccess).Times(1);
    EXPECT_CALL(*policy, OnFinish).Times(0);
    return policy;
  };

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(
      pool.cq(), mock,
      TestOptions(Options{}.set<storage_experimental::ResumePolicyOption>(
          mock_resume_policy_factory)));
  auto pending = connection->ReadObject(
      {google::storage::v2::ReadObjectRequest{}, connection->options()});

  ASSERT_TRUE(pending.is_ready());
  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto reader = *std::move(r);
  auto data = reader->Read();

  // This stream starts successfully:
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  // We expect two `Read()` calls with the same contents and with valid CRC32C
  // values.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  auto response = data.get();
  EXPECT_THAT(response, VariantWith<storage_experimental::ReadPayload>(ResultOf(
                            "payload contents",
                            [](storage_experimental::ReadPayload const& p) {
                              return p.contents();
                            },
                            ElementsAre(std::string(kQuick)))));

  // Trigger the second `Read()` and simulate its behavior.
  data = reader->Read();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  response = data.get();
  EXPECT_THAT(response, VariantWith<storage_experimental::ReadPayload>(ResultOf(
                            "payload contents",
                            [](storage_experimental::ReadPayload const& p) {
                              return p.contents();
                            },
                            ElementsAre(std::string(kQuick)))));

  // The last Read() triggers the end of stream message, including a call to
  // `Finish()`. It should detect the invalid checksum.
  data = reader->Read();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);
  // The stream Finish() function should be called in the background.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  response = data.get();
  EXPECT_THAT(response,
              VariantWith<Status>(StatusIs(StatusCode::kInvalidArgument)));
}

TEST_F(AsyncConnectionImplTest, MakeReaderConnectionFactory) {
  AsyncSequencer<bool> sequencer;
  auto make_success_stream = [&](AsyncSequencer<bool>& sequencer) {
    auto stream = std::make_unique<MockAsyncObjectMediaStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Read).WillOnce([&] {
      return sequencer.PushBack("Read").then([](auto) {
        return absl::optional<google::storage::v2::ReadObjectResponse>{};
      });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncReadObjectStream>(std::move(stream));
  };
  auto verify_empty_stream = [](AsyncSequencer<bool>& sequencer, auto pending) {
    // First simulate a failed `ReadObject()`. This returns a streaming RPC
    // that completes with `false` on `Start()` (i.e. never starts) and then
    // completes with a transient error on `Finish()`.
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value(false);

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value(true);

    // Then simulate a successful `ReadObject()`. To simplify the test we assume
    // this returns no data. The streaming RPC completes with `true` on
    // `Start()`, then an unset optional on `Read()` (indicating 'end of the
    // streaming RPC'), and then a success `Status` for `Finish()`.
    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value(true);

    auto r = pending.get();
    ASSERT_STATUS_OK(r);
    auto reader = *std::move(r);
    auto data = reader->Read();
    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Read");
    next.first.set_value(true);

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value(true);

    EXPECT_THAT(data.get(), VariantWith<Status>(IsOk()));
  };

  auto constexpr kExpectedRequest1 = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    read_offset: 1000
    read_limit: 1000
  )pb";
  auto constexpr kExpectedRequest2 = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    generation: 1234
    read_offset: 1500
    read_limit: 500
  )pb";

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject)
      .WillOnce(
          [&] { return MakeErrorReadStream(sequencer, TransientError()); })
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const& options,
                    google::storage::v2::ReadObjectRequest const& request) {
        // Verify at least one option is initialized with the correct value.
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);
        auto expected = google::storage::v2::ReadObjectRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest1, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return make_success_stream(sequencer);
      })
      .WillOnce(
          [&] { return MakeErrorReadStream(sequencer, TransientError()); })
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const&,
                    google::cloud::internal::ImmutableOptions const& options,
                    google::storage::v2::ReadObjectRequest const& request) {
        // Verify at least one option is initialized with the correct value.
        EXPECT_EQ(options->get<AuthorityOption>(), kAuthority);
        auto expected = google::storage::v2::ReadObjectRequest{};
        EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest2, &expected));
        EXPECT_THAT(request, IsProtoEqual(expected));
        return make_success_stream(sequencer);
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  AsyncConnectionImpl connection(
      pool.cq(), std::shared_ptr<GrpcChannelRefresh>{}, mock, TestOptions());

  auto request = google::storage::v2::ReadObjectRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRequest1, &request));

  auto hash_function = std::make_shared<MockHashFunction>();
  auto factory = connection.MakeReaderConnectionFactory(
      internal::MakeImmutableOptions(connection.options()), std::move(request),
      std::move(hash_function));

  // Verify the factory makes the expected request and consume the output
  verify_empty_stream(sequencer, factory(storage::Generation(), 0));

  verify_empty_stream(sequencer, factory(storage::Generation(1234), 500));
}

TEST_F(AsyncConnectionImplTest, MakeReaderConnectionFactoryPermanentError) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    return MakeErrorReadStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  AsyncConnectionImpl connection(
      pool.cq(), std::shared_ptr<GrpcChannelRefresh>{}, mock, TestOptions());

  auto hash_function = std::make_shared<MockHashFunction>();
  auto factory = connection.MakeReaderConnectionFactory(
      internal::MakeImmutableOptions(connection.options()),
      google::storage::v2::ReadObjectRequest{}, hash_function);

  // Verify the factory makes the expected request.
  auto pending = factory(storage::Generation(), 0);

  auto next = sequencer.PopFrontWithName();
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, MakeReaderConnectionFactoryTooManyTransients) {
  AsyncSequencer<bool> sequencer;

  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).Times(3).WillRepeatedly([&] {
    return MakeErrorReadStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  AsyncConnectionImpl connection(
      pool.cq(), std::shared_ptr<GrpcChannelRefresh>{}, mock, TestOptions());
  auto hash_function = std::make_shared<MockHashFunction>();
  auto factory = connection.MakeReaderConnectionFactory(
      internal::MakeImmutableOptions(connection.options()),
      google::storage::v2::ReadObjectRequest{}, hash_function);
  // Verify the factory makes the expected request.
  auto pending = factory(storage::Generation(), 0);

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value(false);

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value(true);
  }

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(TransientError().code()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
