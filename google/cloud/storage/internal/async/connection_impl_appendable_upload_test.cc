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

#include "google/cloud/storage/async/retry_policy.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockAsyncBidiWriteObjectStream;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;

using AsyncBidiWriteObjectStream = ::google::cloud::AsyncStreamingReadWriteRpc<
    google::storage::v2::BidiWriteObjectRequest,
    google::storage::v2::BidiWriteObjectResponse>;

// A test fixture for the appendable upload tests.
class AsyncConnectionImplAppendableTest : public ::testing::Test {};

// Common options for all tests.
auto TestOptions(Options options = {}) {
  using ms = std::chrono::milliseconds;
  options = internal::MergeOptions(
      std::move(options),
      Options{}
          .set<GrpcNumChannelsOption>(1)
          .set<storage_experimental::AsyncRetryPolicyOption>(
              storage_experimental::LimitedErrorCountRetryPolicy(2).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone()));
  return DefaultOptionsAsync(std::move(options));
}

// Creates a test connection with a mock stub.
std::shared_ptr<storage_experimental::AsyncConnection> MakeTestConnection(
    CompletionQueue cq, std::shared_ptr<storage::testing::MockStorageStub> mock,
    Options options = {}) {
  return MakeAsyncConnection(std::move(cq), std::move(mock),
                             TestOptions(std::move(options)));
}

// Creates a mock bidirectional stream that simulates a successful append flow.
std::unique_ptr<AsyncBidiWriteObjectStream> MakeSuccessfulAppendStream(
    AsyncSequencer<bool>& sequencer, std::int64_t persisted_size) {
  auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
  EXPECT_CALL(*stream, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  // The first write is a "state lookup" write. It should not contain a payload.
  // The server responds with the current persisted size of the object.
  EXPECT_CALL(*stream, Write)
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
        EXPECT_TRUE(request.state_lookup());
        EXPECT_FALSE(wopt.is_last_message());
        return sequencer.PushBack("Write(StateLookup)");
      })
      // Subsequent writes carry data.
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const&,
                    grpc::WriteOptions wopt) {
        EXPECT_FALSE(wopt.is_last_message());
        return sequencer.PushBack("Write(data)");
      })
      // The finalize write marks the end of the stream.
      .WillOnce([&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
        EXPECT_TRUE(request.finish_write());
        EXPECT_TRUE(wopt.is_last_message());
        return sequencer.PushBack("Write(Finalize)");
      });

  // The first `Read()` call after the state lookup confirms the persisted size.
  EXPECT_CALL(*stream, Read)
      .WillOnce([&, persisted_size] {
        return sequencer.PushBack("Read(PersistedSize)")
            .then([persisted_size](auto) {
              auto response = google::storage::v2::BidiWriteObjectResponse{};
              response.mutable_resource()->set_size(persisted_size);
              return absl::make_optional(std::move(response));
            });
      })
      // The second `Read()` call, after the final write, returns the full
      // object metadata.
      .WillOnce([&, persisted_size] {
        return sequencer.PushBack("Read(FinalObject)")
            .then([persisted_size](auto) {
              auto response = google::storage::v2::BidiWriteObjectResponse{};
              response.mutable_resource()->set_bucket(
                  "projects/_/buckets/test-bucket");
              response.mutable_resource()->set_name("test-object");
              // The final size should be greater than the persisted size.
              response.mutable_resource()->set_size(persisted_size + 1024);
              return absl::make_optional(std::move(response));
            });
      });

  EXPECT_CALL(*stream, Cancel).Times(1);
  EXPECT_CALL(*stream, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
  });

  return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
}

// Creates a mock stream that returns an error.
std::unique_ptr<AsyncBidiWriteObjectStream> MakeErrorBidiWriteStream(
    AsyncSequencer<bool>& sequencer, Status const& status) {
  auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
  EXPECT_CALL(*stream, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&, status] {
    return sequencer.PushBack("Finish").then([status](auto) { return status; });
  });
  return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
}

TEST_F(AsyncConnectionImplAppendableTest, StartAppendableObjectUploadSuccess) {
  auto constexpr kRequestText = R"pb(
    write_object_spec {
      resource {
        bucket: "projects/_/buckets/test-bucket"
        name: "test-object"
        content_type: "text/plain"
      }
    }
  )pb";
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();

  // Simulate one transient failure, followed by a success.
  EXPECT_CALL(*mock, AsyncBidiWriteObject)
      .WillOnce(
          [&] { return MakeErrorBidiWriteStream(sequencer, TransientError()); })
      .WillOnce([&] { return MakeSuccessfulAppendStream(sequencer, 0); });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);

  auto request = google::storage::v2::BidiWriteObjectRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending = connection->StartAppendableObjectUpload(
      {std::move(request), connection->options()});

  // First attempt fails.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // The stream fails to start.

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  // Fulfill the promise. The future will complete with the TransientError
  // provided in the mock setup, which the retry loop will handle.
  next.first.set_value(true);

  // Retry attempt succeeds.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write(StateLookup)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read(PersistedSize)");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  // Write some data.
  // An empty payload might be a no-op in the implementation, which would
  // prevent the mock from being triggered and cause the sequencer to hang.
  // We provide a non-empty payload to ensure the Write RPC is sent.
  auto w1 = writer->Write(storage_experimental::WritePayload("some data"));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write(data)");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w1.get());

  // Finalize the upload.
  auto w2 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write(Finalize)");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read(FinalObject)");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "projects/_/buckets/test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->size(), 1024);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplAppendableTest, ResumeAppendableObjectUploadSuccess) {
  auto constexpr kRequestText = R"pb(
    append_object_spec { object: "test-object" }
  )pb";
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();

  // In a resume, the server should report the already persisted size.
  // We'll simulate 16384 bytes are already uploaded.
  constexpr std::int64_t kPersistedSize = 16384;
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&] {
    return MakeSuccessfulAppendStream(sequencer, kPersistedSize);
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);

  auto request = google::storage::v2::BidiWriteObjectRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending = connection->ResumeAppendableObjectUpload(
      {std::move(request), connection->options()});

  // The stream starts, performs state lookup, and reports the persisted size.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write(StateLookup)");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read(PersistedSize)");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  // Verify the persisted state is correctly reported.
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), kPersistedSize);

  auto w1 = writer->Write(storage_experimental::WritePayload("some more data"));
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write(data)");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w1.get());

  // Finalize the upload.
  auto w2 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write(Finalize)");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read(FinalObject)");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->size(), kPersistedSize + 1024);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplAppendableTest, AppendableUploadTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  // The retry policy is configured for 3 attempts total.
  EXPECT_CALL(*mock, AsyncBidiWriteObject).Times(3).WillRepeatedly([&] {
    return MakeErrorBidiWriteStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartAppendableObjectUpload(
      {google::storage::v2::BidiWriteObjectRequest{}, connection->options()});

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

TEST_F(AsyncConnectionImplAppendableTest, AppendableUploadPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&] {
    return MakeErrorBidiWriteStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartAppendableObjectUpload(
      {google::storage::v2::BidiWriteObjectRequest{}, connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(PermanentError().code()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
