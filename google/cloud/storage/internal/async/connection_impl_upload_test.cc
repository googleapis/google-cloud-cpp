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

#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/async/connection_impl.h"
#include "google/cloud/storage/internal/async/default_options.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/testing/canonical_errors.h"
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

using ::google::cloud::storage::testing::MockAsyncBidiWriteObjectStream;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::protobuf::TextFormat;

using AsyncBidiWriteObjectStream = ::google::cloud::AsyncStreamingReadWriteRpc<
    google::storage::v2::BidiWriteObjectRequest,
    google::storage::v2::BidiWriteObjectResponse>;

class AsyncConnectionImplTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

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

TEST_F(AsyncConnectionImplTest, UnbufferedUploadNewUpload) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite)
      .WillOnce([&] {
        return sequencer.PushBack("StartResumableWrite(1)").then([](auto) {
          return StatusOr<google::storage::v2::StartResumableWriteResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::StartResumableWriteRequest const& request) {
            auto const& spec = request.write_object_spec();
            EXPECT_TRUE(spec.has_if_generation_match());
            EXPECT_EQ(spec.if_generation_match(), 123);
            auto const& resource = spec.resource();
            EXPECT_EQ(resource.bucket(), "projects/_/buckets/test-bucket");
            EXPECT_EQ(resource.name(), "test-object");
            EXPECT_EQ(resource.content_type(), "text/plain");

            return sequencer.PushBack("StartResumableWrite(2)").then([](auto) {
              auto response =
                  google::storage::v2::StartResumableWriteResponse{};
              response.set_upload_id("test-upload-id");
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject)
      .WillOnce(
          [&] { return MakeErrorBidiWriteStream(sequencer, TransientError()); })
      .WillOnce([&]() {
        auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start");
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_TRUE(request.has_upload_id());
                  EXPECT_EQ(request.upload_id(), "test-upload-id");
                  EXPECT_FALSE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                })
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_FALSE(request.has_upload_id());
                  EXPECT_TRUE(request.finish_write());
                  EXPECT_TRUE(request.has_object_checksums());
                  EXPECT_TRUE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                });
        EXPECT_CALL(*stream, Read).WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            auto response = google::storage::v2::BidiWriteObjectResponse{};
            response.mutable_resource()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_resource()->set_name("test-object");
            response.mutable_resource()->set_generation(123456);
            return absl::make_optional(std::move(response));
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });
        return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  // We will configure the connection to use disable timeouts, this is just used
  // for the retry loop backoff timers.
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
                         Options{}.set<storage::TransferStallTimeoutOption>(
                             std::chrono::seconds(0)));
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::WithObjectMetadata(
                   storage::ObjectMetadata{}.set_content_type("text/plain")),
               storage::IfGenerationMatch(123)),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(1)");
  next.first.set_value(true);

  // The retry loop should start a backoff timer.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "MakeRelativeTimer");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(2)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // The first stream fails

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  // The retry loop should start a backoff timer.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "MakeRelativeTimer");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w1 = writer->Write({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w1.get());

  auto w2 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, UnbufferedUploadNewUploadWithTimeout) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite).WillOnce([&sequencer] {
    return sequencer.PushBack("StartResumableWrite").then([](auto) {
      auto response = google::storage::v2::StartResumableWriteResponse{};
      response.set_upload_id("test-upload-id");
      return make_status_or(response);
    });
  });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&]() {
    auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer] {
      return sequencer.PushBack("Start");
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer] {
      return sequencer.PushBack("Write");
    });
    EXPECT_CALL(*stream, Read).WillOnce([&sequencer] {
      return sequencer.PushBack("Read").then([](auto) {
        auto response = google::storage::v2::BidiWriteObjectResponse{};
        response.mutable_resource()->set_bucket(
            "projects/_/buckets/test-bucket");
        response.mutable_resource()->set_name("test-object");
        response.mutable_resource()->set_generation(123456);
        return absl::make_optional(std::move(response));
      });
    });
    EXPECT_CALL(*stream, Cancel).Times(1);
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer] {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
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
          .set<storage::TransferStallTimeoutOption>(std::chrono::seconds(1))
          .set<storage::TransferStallMinimumRateOption>(2 * 1024 * 1024L));
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::WithObjectMetadata(
                   storage::ObjectMetadata{}.set_content_type("text/plain")),
               storage::IfGenerationMatch(123)),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite");
  next.first.set_value(true);

  auto timer = sequencer.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  timer.first.set_value(false);  // simulate a cancelled timer.
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w2 = writer->Finalize({});
  timer = sequencer.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  timer.first.set_value(false);  // simulate a cancelled timer.
  next.first.set_value(true);
  timer = sequencer.PopFrontWithName();
  EXPECT_EQ(timer.second, "MakeRelativeTimer");
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  timer.first.set_value(false);  // simulate a cancelled timer.
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, ResumeUnbufferedUpload) {
  // The placeholder values in the `common_object_request_params` are just
  // canaries to verify the full request is passed along.
  auto constexpr kRequestText = R"pb(
    upload_id: "test-upload-id"
    common_object_request_params {
      encryption_algorithm: "test-ea"
      encryption_key_bytes: "test-ekb"
      encryption_key_sha256_bytes: "test-eksb"
    }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce([&] {
        return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
          return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            auto expected = google::storage::v2::QueryWriteStatusRequest{};
            EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &expected));
            EXPECT_THAT(request, IsProtoEqual(expected));

            return sequencer.PushBack("QueryWriteStatus(2)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(16384);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject)
      .WillOnce(
          [&] { return MakeErrorBidiWriteStream(sequencer, TransientError()); })
      .WillOnce([&]() {
        auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start");
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_TRUE(request.has_upload_id());
                  EXPECT_EQ(request.upload_id(), "test-upload-id");
                  EXPECT_FALSE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                })
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_FALSE(request.has_upload_id());
                  EXPECT_TRUE(request.finish_write());
                  EXPECT_TRUE(request.has_object_checksums());
                  EXPECT_TRUE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                });
        EXPECT_CALL(*stream, Read).WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            auto response = google::storage::v2::BidiWriteObjectResponse{};
            response.mutable_resource()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_resource()->set_name("test-object");
            response.mutable_resource()->set_generation(123456);
            return absl::make_optional(std::move(response));
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });
        return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(2)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // The first stream fails

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 16384);

  auto w1 = writer->Write({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w1.get());

  auto w2 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, ResumeUnbufferedUploadFinalized) {
  auto constexpr kRequestText = R"pb(
    upload_id: "test-upload-id"
    common_object_request_params {
      encryption_algorithm: "test-ea"
      encryption_key_bytes: "test-ekb"
      encryption_key_sha256_bytes: "test-eksb"
    }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce([&] {
        return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
          return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            auto expected = google::storage::v2::QueryWriteStatusRequest{};
            EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &expected));
            EXPECT_THAT(request, IsProtoEqual(expected));

            return sequencer.PushBack("QueryWriteStatus(2)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.mutable_resource()->set_bucket(
                  "projects/_/buckets/test-bucket");
              response.mutable_resource()->set_name("test-object");
              response.mutable_resource()->set_generation(123456);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).Times(0);

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(2)");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  ASSERT_TRUE(absl::holds_alternative<storage::ObjectMetadata>(
      writer->PersistedState()));
  auto metadata = absl::get<storage::ObjectMetadata>(writer->PersistedState());
  EXPECT_EQ(metadata.bucket(), "test-bucket");
  EXPECT_EQ(metadata.name(), "test-object");
  EXPECT_EQ(metadata.generation(), 123456);

  writer.reset();
}

TEST_F(AsyncConnectionImplTest,
       UnbufferedUploadTooManyTransientsOnStartResumableWrite) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite).Times(3).WillRepeatedly([&] {
    return sequencer.PushBack("StartResumableWrite").then([](auto) {
      return StatusOr<google::storage::v2::StartResumableWriteResponse>(
          TransientError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object"),
       connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "StartResumableWrite");
    next.first.set_value(false);
  }

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(TransientError().code()));
}

TEST_F(AsyncConnectionImplTest,
       UnbufferedUploadPermanentErrorOnStartResumableWrite) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite).WillOnce([&] {
    return sequencer.PushBack("StartResumableWrite").then([](auto) {
      return StatusOr<google::storage::v2::StartResumableWriteResponse>(
          PermanentError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object"),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite");
  next.first.set_value(false);

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, UnbufferedUploadInvalidRequest) {
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite).Times(0);

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  // Intentionally create an invalid key. Converting this key to a proto message
  // will fail, and that should result in an error.
  auto key = storage::EncryptionDataFromBinaryKey("123");
  key.sha256 = "not-a-valid-base-64-SHA256";
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(storage::EncryptionKey(key)),
       connection->options()});

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(AsyncConnectionImplTest, ResumeUnbufferedUploadTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus).Times(3).WillRepeatedly([&] {
    return sequencer.PushBack("QueryWriteStatus").then([](auto) {
      return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
          TransientError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "QueryWriteStatus");
    next.first.set_value(false);
  }

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(TransientError().code()));
}

TEST_F(AsyncConnectionImplTest, ResumeUnbufferedUploadPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus).WillOnce([&] {
    return sequencer.PushBack("QueryWriteStatus").then([](auto) {
      return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
          PermanentError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus");
  next.first.set_value(false);

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, UnbufferedUploadTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite).WillOnce([] {
    auto response = google::storage::v2::StartResumableWriteResponse{};
    response.set_upload_id("test-upload-id");
    return make_ready_future(make_status_or(response));
  });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).Times(3).WillRepeatedly([&] {
    return MakeErrorBidiWriteStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object"),
       connection->options()});

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

TEST_F(AsyncConnectionImplTest, UnbufferedUploadPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite).WillOnce([] {
    auto response = google::storage::v2::StartResumableWriteResponse{};
    response.set_upload_id("test-upload-id");
    return make_ready_future(make_status_or(response));
  });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).WillOnce([&] {
    return MakeErrorBidiWriteStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object"),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, BufferedUploadNewUpload) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite)
      .WillOnce([&] {
        return sequencer.PushBack("StartResumableWrite(1)").then([](auto) {
          return StatusOr<google::storage::v2::StartResumableWriteResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::StartResumableWriteRequest const& request) {
            auto const& spec = request.write_object_spec();
            EXPECT_TRUE(spec.has_if_generation_match());
            EXPECT_EQ(spec.if_generation_match(), 123);
            auto const& resource = spec.resource();
            EXPECT_EQ(resource.bucket(), "projects/_/buckets/test-bucket");
            EXPECT_EQ(resource.name(), "test-object");
            EXPECT_EQ(resource.content_type(), "text/plain");

            return sequencer.PushBack("StartResumableWrite(2)").then([](auto) {
              auto response =
                  google::storage::v2::StartResumableWriteResponse{};
              response.set_upload_id("test-upload-id");
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject)
      .WillOnce(
          [&] { return MakeErrorBidiWriteStream(sequencer, TransientError()); })
      .WillOnce([&]() {
        auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start");
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_EQ(request.upload_id(), "test-upload-id");
                  EXPECT_TRUE(request.finish_write());
                  EXPECT_TRUE(request.has_object_checksums());
                  EXPECT_TRUE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                });
        EXPECT_CALL(*stream, Read).WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            auto response = google::storage::v2::BidiWriteObjectResponse{};
            response.mutable_resource()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_resource()->set_name("test-object");
            response.mutable_resource()->set_generation(123456);
            return absl::make_optional(std::move(response));
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });
        return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartBufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::WithObjectMetadata(
                   storage::ObjectMetadata{}.set_content_type("text/plain")),
               storage::IfGenerationMatch(123)),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(2)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // The first stream fails

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w1 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w1.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, ResumeBufferedUploadNewUploadResume) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncStartResumableWrite)
      .WillOnce([&] {
        return sequencer.PushBack("StartResumableWrite(1)").then([](auto) {
          return StatusOr<google::storage::v2::StartResumableWriteResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::StartResumableWriteRequest const& request) {
            auto const& spec = request.write_object_spec();
            EXPECT_TRUE(spec.has_if_generation_match());
            EXPECT_EQ(spec.if_generation_match(), 123);
            auto const& resource = spec.resource();
            EXPECT_EQ(resource.bucket(), "projects/_/buckets/test-bucket");
            EXPECT_EQ(resource.name(), "test-object");
            EXPECT_EQ(resource.content_type(), "text/plain");

            return sequencer.PushBack("StartResumableWrite(2)").then([](auto) {
              auto response =
                  google::storage::v2::StartResumableWriteResponse{};
              response.set_upload_id("test-upload-id");
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce([&] {
        return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
          return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "test-upload-id");

            return sequencer.PushBack("QueryWriteStatus(2)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(0);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject)
      .WillOnce(
          [&] { return MakeErrorBidiWriteStream(sequencer, TransientError()); })
      .WillOnce([&]() {
        auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start(1)");
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_EQ(request.upload_id(), "test-upload-id");
                  EXPECT_TRUE(request.finish_write());
                  EXPECT_TRUE(request.has_object_checksums());
                  EXPECT_TRUE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                });
        EXPECT_CALL(*stream, Read).WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            return absl::optional<
                google::storage::v2::BidiWriteObjectResponse>{};
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then(
              [](auto) { return TransientError(); });
        });
        return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
      })
      .WillOnce([&]() {
        auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start(2)");
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_EQ(request.upload_id(), "test-upload-id");
                  EXPECT_TRUE(request.finish_write());
                  EXPECT_TRUE(request.has_object_checksums());
                  EXPECT_TRUE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                });
        EXPECT_CALL(*stream, Read).WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            auto response = google::storage::v2::BidiWriteObjectResponse{};
            response.mutable_resource()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_resource()->set_name("test-object");
            response.mutable_resource()->set_generation(123456);
            return absl::make_optional(std::move(response));
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });
        return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->StartBufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::WithObjectMetadata(
                   storage::ObjectMetadata{}.set_content_type("text/plain")),
               storage::IfGenerationMatch(123)),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "StartResumableWrite(2)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // The first stream fails

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start(1)");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 0);

  auto w1 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(2)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start(2)");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w1.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, ResumeBufferedUpload) {
  // The placeholder values in the `common_object_request_params` are just
  // canaries to verify the full request is passed along.
  auto constexpr kRequestText = R"pb(
    upload_id: "test-upload-id"
    common_object_request_params {
      encryption_algorithm: "test-ea"
      encryption_key_bytes: "test-ekb"
      encryption_key_sha256_bytes: "test-eksb"
    }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce([&] {
        return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
          return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            auto expected = google::storage::v2::QueryWriteStatusRequest{};
            EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &expected));
            EXPECT_THAT(request, IsProtoEqual(expected));

            return sequencer.PushBack("QueryWriteStatus(2)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.set_persisted_size(16384);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject)
      .WillOnce(
          [&] { return MakeErrorBidiWriteStream(sequencer, TransientError()); })
      .WillOnce([&]() {
        auto stream = std::make_unique<MockAsyncBidiWriteObjectStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start");
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_TRUE(request.has_upload_id());
                  EXPECT_EQ(request.upload_id(), "test-upload-id");
                  EXPECT_FALSE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                })
            .WillOnce(
                [&](google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions wopt) {
                  EXPECT_FALSE(request.has_upload_id());
                  EXPECT_TRUE(request.finish_write());
                  EXPECT_TRUE(request.has_object_checksums());
                  EXPECT_TRUE(wopt.is_last_message());
                  return sequencer.PushBack("Write");
                });
        EXPECT_CALL(*stream, Read).WillOnce([&] {
          return sequencer.PushBack("Read").then([](auto) {
            auto response = google::storage::v2::BidiWriteObjectResponse{};
            response.mutable_resource()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_resource()->set_name("test-object");
            response.mutable_resource()->set_generation(123456);
            return absl::make_optional(std::move(response));
          });
        });
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then(
              [](auto) { return Status{}; });
        });
        return std::unique_ptr<AsyncBidiWriteObjectStream>(std::move(stream));
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending = connection->ResumeUnbufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(2)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // The first stream fails

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  EXPECT_EQ(absl::get<std::int64_t>(writer->PersistedState()), 16384);

  auto w1 = writer->Write({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  EXPECT_STATUS_OK(w1.get());

  auto w2 = writer->Finalize({});
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = w2.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->generation(), 123456);

  writer.reset();
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST_F(AsyncConnectionImplTest, ResumeBufferedUploadFinalized) {
  auto constexpr kRequestText = R"pb(
    upload_id: "test-upload-id"
    common_object_request_params {
      encryption_algorithm: "test-ea"
      encryption_key_bytes: "test-ekb"
      encryption_key_sha256_bytes: "test-eksb"
    }
  )pb";

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus)
      .WillOnce([&] {
        return sequencer.PushBack("QueryWriteStatus(1)").then([](auto) {
          return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
              TransientError());
        });
      })
      .WillOnce(
          [&](auto&, auto, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            auto expected = google::storage::v2::QueryWriteStatusRequest{};
            EXPECT_TRUE(TextFormat::ParseFromString(kRequestText, &expected));
            EXPECT_THAT(request, IsProtoEqual(expected));

            return sequencer.PushBack("QueryWriteStatus(2)").then([](auto) {
              auto response = google::storage::v2::QueryWriteStatusResponse{};
              response.mutable_resource()->set_bucket(
                  "projects/_/buckets/test-bucket");
              response.mutable_resource()->set_name("test-object");
              response.mutable_resource()->set_generation(123456);
              return make_status_or(response);
            });
          });
  EXPECT_CALL(*mock, AsyncBidiWriteObject).Times(0);

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));
  auto pending = connection->ResumeBufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(1)");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus(2)");
  next.first.set_value(true);

  auto r = pending.get();
  ASSERT_STATUS_OK(r);
  auto writer = *std::move(r);
  EXPECT_EQ(writer->UploadId(), "test-upload-id");
  ASSERT_TRUE(absl::holds_alternative<storage::ObjectMetadata>(
      writer->PersistedState()));
  auto metadata = absl::get<storage::ObjectMetadata>(writer->PersistedState());
  EXPECT_EQ(metadata.bucket(), "test-bucket");
  EXPECT_EQ(metadata.name(), "test-object");
  EXPECT_EQ(metadata.generation(), 123456);

  writer.reset();
}

TEST_F(AsyncConnectionImplTest, ResumeBufferedUploadTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus).Times(3).WillRepeatedly([&] {
    return sequencer.PushBack("QueryWriteStatus").then([](auto) {
      return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
          TransientError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeBufferedUpload(
      {std::move(request), connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "QueryWriteStatus");
    next.first.set_value(false);
  }

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(TransientError().code()));
}

TEST_F(AsyncConnectionImplTest, ResumeBufferedUploadPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncQueryWriteStatus).WillOnce([&] {
    return sequencer.PushBack("QueryWriteStatus").then([](auto) {
      return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
          PermanentError());
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto request = google::storage::v2::QueryWriteStatusRequest{};
  request.set_upload_id("resume-upload-id");
  auto pending = connection->ResumeBufferedUpload(
      {std::move(request), connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "QueryWriteStatus");
  next.first.set_value(false);

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(PermanentError().code()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
