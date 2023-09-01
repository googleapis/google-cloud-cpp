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
using ::google::cloud::storage::testing::MockAsyncInsertStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using AsyncWriteObjectStream =
    ::google::cloud::internal::AsyncStreamingWriteRpc<
        google::storage::v2::WriteObjectRequest,
        google::storage::v2::WriteObjectResponse>;

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

std::shared_ptr<AsyncConnection> MakeTestConnection(
    CompletionQueue cq, std::shared_ptr<storage::testing::MockStorageStub> mock,
    Options options = {}) {
  using namespace std::chrono_literals;  // NOLINT(google-using-directives)
  options = internal::MergeOptions(
      std::move(options),
      Options{}
          .set<storage::RetryPolicyOption>(
              storage::LimitedErrorCountRetryPolicy(2).clone())
          .set<storage::BackoffPolicyOption>(
              storage::ExponentialBackoffPolicy(1ms, 2ms, 2.0).clone()));
  return MakeAsyncConnection(std::move(cq), std::move(mock),
                             DefaultOptionsGrpc(std::move(options)));
}

std::unique_ptr<AsyncWriteObjectStream> MakeErrorStream(
    AsyncSequencer<bool>& sequencer, Status status) {
  auto stream = std::make_unique<MockAsyncInsertStream>();
  EXPECT_CALL(*stream, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*stream, Finish).WillOnce([&, status] {
    return sequencer.PushBack("Finish").then([&status](auto) {
      return StatusOr<google::storage::v2::WriteObjectResponse>(status);
    });
  });
  return std::unique_ptr<AsyncWriteObjectStream>(std::move(stream));
}

TEST_F(AsyncConnectionImplTest, AsyncInsertObject) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce([&] {
        // Force at least one retry before verifying it works with successful
        // requests.
        return MakeErrorStream(sequencer, TransientError());
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
  auto pending = connection->AsyncInsertObject(
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
    return MakeErrorStream(sequencer, PermanentError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->AsyncInsertObject(
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
    return MakeErrorStream(sequencer, TransientError());
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->AsyncInsertObject(
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

TEST_F(AsyncConnectionImplTest, AsyncDeleteObject) {
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
  auto pending = connection->AsyncDeleteObject(
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

TEST_F(AsyncConnectionImplTest, AsyncDeleteObjectPermanentError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<storage::testing::MockStorageStub>();
  EXPECT_CALL(*mock, AsyncDeleteObject).WillOnce([&] {
    return sequencer.PushBack("DeleteObject").then([](auto) {
      return PermanentError();
    });
  });

  internal::AutomaticallyCreatedBackgroundThreads pool(1);
  auto connection = MakeTestConnection(pool.cq(), mock);
  auto pending = connection->AsyncDeleteObject(
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
  auto pending = connection->AsyncDeleteObject(
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
