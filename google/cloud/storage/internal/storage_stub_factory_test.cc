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

#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockInsertStream;
using ::google::cloud::storage::testing::MockObjectMediaStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Return;

// The point of these tests is to verify that the `CreateStorageStub` factory
// function injects the right decorators. We do this by observing the
// side effects of these decorators. All the tests have nearly identical
// structure. They create a fully decorated stub, configured to round-robin
// over kTestChannel mocks.  The first mock expects a call, the remaining mocks
// expect no calls.  Some of these side effects can only be verified as part of
// the first mock.

class StorageStubFactory : public ::testing::Test {
 protected:
  Status IsContextMDValid(grpc::ClientContext& context,
                          std::string const& method) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, google::cloud::internal::ApiClientHeader("generator"));
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kTestChannels = 3;

using MockFactory = ::testing::MockFunction<std::shared_ptr<StorageStub>(
    std::shared_ptr<grpc::Channel>)>;
static_assert(std::is_same<decltype(MockFactory{}.AsStdFunction()),
                           BaseStorageStubFactory>::value,
              "Mismatched mock factory type");

std::shared_ptr<StorageStub> CreateTestStub(
    CompletionQueue cq, BaseStorageStubFactory const& factory) {
  auto credentials = google::cloud::MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::minutes(5));
  return CreateDecoratedStubs(std::move(cq),
                              google::cloud::Options{}
                                  .set<GrpcNumChannelsOption>(kTestChannels)
                                  .set<TracingComponentsOption>({"rpc"})
                                  .set<UnifiedCredentialsOption>(credentials),
                              factory);
}

TEST_F(StorageStubFactory, ReadObject) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, ReadObject)
            .WillOnce([this](std::unique_ptr<grpc::ClientContext> context,
                             google::storage::v2::ReadObjectRequest const&) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context->credentials(), NotNull());
              // Verify the Metadata decorator is present
              EXPECT_STATUS_OK(IsContextMDValid(
                  *context, "google.storage.v2.Storage.ReadObject"));
              auto stream = absl::make_unique<MockObjectMediaStream>();
              EXPECT_CALL(*stream, Read)
                  .WillOnce(
                      Return(Status(StatusCode::kUnavailable, "nothing here")));
              return stream;
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockStorageStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto stream = stub->ReadObject(absl::make_unique<grpc::ClientContext>(),
                                 google::storage::v2::ReadObjectRequest{});
  auto read = stream->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(read));
  EXPECT_THAT(absl::get<Status>(read), StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("ReadObject")));
}

TEST_F(StorageStubFactory, WriteObject) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, WriteObject)
            .WillOnce([this](std::unique_ptr<grpc::ClientContext> context) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context->credentials(), NotNull());
              // Verify the Metadata decorator is present
              EXPECT_STATUS_OK(IsContextMDValid(
                  *context, "google.storage.v2.Storage.WriteObject"));
              auto stream = absl::make_unique<MockInsertStream>();
              EXPECT_CALL(*stream, Close)
                  .WillOnce(
                      Return(StatusOr<google::storage::v2::WriteObjectResponse>(
                          Status(StatusCode::kUnavailable, "nothing here"))));
              return stream;
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockStorageStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto stream = stub->WriteObject(absl::make_unique<grpc::ClientContext>());
  auto close = stream->Close();
  EXPECT_THAT(close, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("WriteObject")));
}

TEST_F(StorageStubFactory, StartResumableWrite) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, StartResumableWrite)
            .WillOnce(
                [this](grpc::ClientContext& context,
                       google::storage::v2::StartResumableWriteRequest const&) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  EXPECT_STATUS_OK(IsContextMDValid(
                      context,
                      "google.storage.v2.Storage.StartResumableWrite"));
                  return StatusOr<
                      google::storage::v2::StartResumableWriteResponse>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockStorageStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  grpc::ClientContext context;
  auto response = stub->StartResumableWrite(
      context, google::storage::v2::StartResumableWriteRequest{});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("StartResumableWrite")));
}

TEST_F(StorageStubFactory, QueryWriteStatus) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, QueryWriteStatus)
            .WillOnce([this](
                          grpc::ClientContext& context,
                          google::storage::v2::QueryWriteStatusRequest const&) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              // Verify the Metadata decorator is present
              EXPECT_STATUS_OK(IsContextMDValid(
                  context, "google.storage.v2.Storage.QueryWriteStatus"));
              return StatusOr<google::storage::v2::QueryWriteStatusResponse>(
                  Status(StatusCode::kUnavailable, "nothing here"));
            });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockStorageStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  grpc::ClientContext context;
  auto response = stub->QueryWriteStatus(
      context, google::storage::v2::QueryWriteStatusRequest{});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("QueryWriteStatus")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
