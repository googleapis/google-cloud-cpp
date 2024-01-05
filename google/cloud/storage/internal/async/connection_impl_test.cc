// Copyright 2022 Google LLC
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
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::storage::testing::MockAsyncBidiWriteObjectStream;
using ::google::cloud::storage::testing::MockAsyncInsertStream;
using ::google::cloud::storage::testing::MockAsyncObjectMediaStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using AsyncWriteObjectStream =
    ::google::cloud::internal::AsyncStreamingWriteRpc<
        google::storage::v2::WriteObjectRequest,
        google::storage::v2::WriteObjectResponse>;

using AsyncReadObjectStream = ::google::cloud::internal::AsyncStreamingReadRpc<
    google::storage::v2::ReadObjectResponse>;

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

auto constexpr kAuthority = "storage.googleapis.com";

std::shared_ptr<storage_experimental::AsyncConnection> MakeTestConnection(
    CompletionQueue cq, std::shared_ptr<storage::testing::MockStorageStub> mock,
    Options options = {}) {
  using ms = std::chrono::milliseconds;
  options = internal::MergeOptions(
      std::move(options),
      Options{}
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(2).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(ms(1), ms(2), 2.0).clone()));
  return MakeAsyncConnection(std::move(cq), std::move(mock),
                             DefaultOptionsGrpc(std::move(options)));
}

std::unique_ptr<AsyncWriteObjectStream> MakeErrorInsertStream(
    AsyncSequencer<bool>& sequencer, Status const& status) {
  auto stream = std::make_unique<MockAsyncInsertStream>();
  EXPECT_CALL(*stream, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&, status] {
    return sequencer.PushBack("Finish").then([status](auto) {
      return StatusOr<google::storage::v2::WriteObjectResponse>(status);
    });
  });
  return std::unique_ptr<AsyncWriteObjectStream>(std::move(stream));
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

TEST_F(AsyncConnectionImplTest, AsyncInsertObject) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce([&] {
        // Force at least one retry before verifying it works with successful
        // requests.
        return MakeErrorInsertStream(sequencer, TransientError());
      })
      .WillOnce([&](CompletionQueue const&,
                    // NOLINTNEXTLINE(performance-unnecessary-value-param)
                    std::shared_ptr<grpc::ClientContext> context) {
        // TODO(#12359) - use the explicit `options` when available.
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2"),
                        Pair("x-goog-request-params",
                             "bucket=projects%2F_%2Fbuckets%2Ftest-bucket"),
                        Pair("x-goog-gcs-idempotency-token", _)));
        auto stream = std::make_unique<MockAsyncInsertStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return sequencer.PushBack("Start");
        });
        EXPECT_CALL(*stream, Write).WillOnce([&](auto const& request, auto) {
          EXPECT_TRUE(request.has_write_object_spec());
          auto const& resource = request.write_object_spec().resource();
          EXPECT_THAT(resource.bucket(), "projects/_/buckets/test-bucket");
          EXPECT_THAT(resource.name(), "test-object");
          return sequencer.PushBack("Write");
        });
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return sequencer.PushBack("Finish").then([](auto) {
            google::storage::v2::WriteObjectResponse response;
            response.mutable_resource()->set_bucket(
                "projects/_/buckets/test-bucket");
            response.mutable_resource()->set_name("test-object");
            response.mutable_resource()->set_size(123456);
            return make_status_or(response);
          });
        });
        EXPECT_CALL(*stream, GetRequestMetadata);
        return std::unique_ptr<AsyncWriteObjectStream>(std::move(stream));
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->InsertObject(
      {storage_experimental::InsertObjectRequest("test-bucket", "test-object")
           .set_multiple_options(storage::Fields("field1,field2"),
                                 storage::QuotaUser("test-quota-user")),
       storage_experimental::WritePayload(),
       /*.options=*/connection->options()});

  // Simulate a transient failure.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  // Simulate a successful request.
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->bucket(), "test-bucket");
  EXPECT_EQ(response->name(), "test-object");
  EXPECT_EQ(response->size(), 123456);
  EXPECT_THAT(response->self_link(), HasSubstr("test-object"));
}

TEST_F(AsyncConnectionImplTest, AsyncInsertObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject).WillOnce([&] {
    return MakeErrorInsertStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->InsertObject(
      {storage_experimental::InsertObjectRequest("test-bucket", "test-object")
           .set_multiple_options(storage::Fields("field1,field2"),
                                 storage::QuotaUser("test-quota-user")),
       storage_experimental::WritePayload(),
       /*.options=*/connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(false);

  auto response = pending.get();
  ASSERT_THAT(response, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, AsyncInsertObjectTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject).Times(3).WillRepeatedly([&] {
    return MakeErrorInsertStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->InsertObject(
      {storage_experimental::InsertObjectRequest("test-bucket", "test-object")
           .set_multiple_options(storage::Fields("field1,field2"),
                                 storage::QuotaUser("test-quota-user")),
       storage_experimental::WritePayload(),
       /*.options=*/connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Start");
    next.first.set_value(false);

    next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "Finish");
    next.first.set_value(false);
  }

  auto response = pending.get();
  ASSERT_THAT(response, StatusIs(TransientError().code()));
}

TEST_F(AsyncConnectionImplTest, AsyncReadObject) {
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
                    // NOLINTNEXTLINE(performance-unnecessary-value-param)
                    std::shared_ptr<grpc::ClientContext> context,
                    google::storage::v2::ReadObjectRequest const& request) {
        // Verify at least one option is initialized with the correct
        // values.
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return make_success_stream(sequencer);
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ReadObject(
      {storage_experimental::ReadObjectRequest("test-bucket", "test-object")
           .set_multiple_options(storage::Fields("field1,field2"),
                                 storage::QuotaUser("test-quota-user")),
       connection->options()});

  // First simulate a failed `ReadObject()`. This returns a streaming RPC
  // that completes with `false` on `Start()` (i.e. never starts) and then
  // completes with a transient error on `Finish()`.
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  // Then simulate a successful `ReadObject()`. This returns a streaming
  // RPC that completes with `true` on `Start()`, then returns some data on the
  // first `Read()`, then an unset optional on the second `Read()` (indicating
  // 'end of the streaming RPC'), and then a success `Status` for `Finish()`.
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

TEST_F(AsyncConnectionImplTest, AsyncReadObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).WillOnce([&] {
    return MakeErrorReadStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ReadObject(
      {storage_experimental::ReadObjectRequest("test-bucket", "test-object"),
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

TEST_F(AsyncConnectionImplTest, AsyncReadObjectTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncReadObject).Times(3).WillRepeatedly([&] {
    return MakeErrorReadStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->ReadObject(
      {storage_experimental::ReadObjectRequest("test-bucket", "test-object"),
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
          [&](auto&, auto,
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

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
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

TEST_F(AsyncConnectionImplTest, UnbufferedUploadResumeUpload) {
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
          [&](auto&, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "test-upload-id");

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
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::UseResumableUploadSession("test-upload-id")),
       connection->options()});

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

TEST_F(AsyncConnectionImplTest, UnbufferedUploadResumeFinalizedUpload) {
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
          [&](auto&, auto,
              google::storage::v2::QueryWriteStatusRequest const& request) {
            EXPECT_EQ(request.upload_id(), "test-upload-id");

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
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::UseResumableUploadSession("test-upload-id")),
       connection->options()});

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
  // will fail, and that should result in a
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

TEST_F(AsyncConnectionImplTest,
       UnbufferedUploadTooManyTransientsOnQueryWriteStatus) {
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
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::UseResumableUploadSession("test-upload-id")),
       connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "QueryWriteStatus");
    next.first.set_value(false);
  }

  auto r = pending.get();
  EXPECT_THAT(r, StatusIs(TransientError().code()));
}

TEST_F(AsyncConnectionImplTest,
       UnbufferedUploadPermanentErrorOnQueryWriteStatus) {
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
  auto pending = connection->StartUnbufferedUpload(
      {storage_experimental::ResumableUploadRequest("test-bucket",
                                                    "test-object")
           .set_multiple_options(
               storage::UseResumableUploadSession("test-upload-id")),
       connection->options()});

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
          [&](auto&, auto,
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

TEST_F(AsyncConnectionImplTest, BufferedUploadNewUploadResume) {
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
          [&](auto&, auto,
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
          [&](auto&, auto,
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

TEST_F(AsyncConnectionImplTest, DeleteObject) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject)
      .WillOnce([&] {
        return sequencer.PushBack("DeleteObject(1)").then([](auto) {
          return TransientError();
        });
      })
      .WillOnce([&](CompletionQueue&, auto context,
                    google::storage::v2::DeleteObjectRequest const& request) {
        // Verify at least one option is initialized with the correct
        // values.
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return sequencer.PushBack("DeleteObject(2)").then([](auto) {
          return Status{};
        });
      });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->DeleteObject(
      {storage_experimental::DeleteObjectRequest("test-bucket", "test-object")
           .set_multiple_options(storage::Fields("field1,field2"),
                                 storage::QuotaUser("test-quota-user")),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "DeleteObject(1)");
  next.first.set_value(false);

  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "DeleteObject(2)");
  next.first.set_value(true);

  auto response = pending.get();
  ASSERT_STATUS_OK(response);
}

TEST_F(AsyncConnectionImplTest, DeleteObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject).WillOnce([&] {
    return sequencer.PushBack("DeleteObject").then([](auto) {
      return PermanentError();
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->DeleteObject(
      {storage_experimental::DeleteObjectRequest("test-bucket", "test-object"),
       connection->options()});

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "DeleteObject");
  next.first.set_value(false);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST_F(AsyncConnectionImplTest, AsyncDeleteObjectTooManyTransients) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject).Times(3).WillRepeatedly([&] {
    return sequencer.PushBack("DeleteObject").then([](auto) {
      return TransientError();
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->DeleteObject(
      {storage_experimental::DeleteObjectRequest("test-bucket", "test-object"),
       connection->options()});

  for (int i = 0; i != 3; ++i) {
    auto next = sequencer.PopFrontWithName();
    EXPECT_EQ(next.second, "DeleteObject");
    next.first.set_value(false);
  }

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(TransientError().code()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
