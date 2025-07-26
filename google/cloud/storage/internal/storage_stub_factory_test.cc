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
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockAsyncInsertStream;
using ::google::cloud::storage::testing::MockObjectMediaStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::VariantWith;

// The point of these tests is to verify that the `CreateStorageStub` factory
// function injects the right decorators. We do this by observing the
// side effects of these decorators. All the tests have nearly identical
// structure. They create a fully decorated stub, configured to round-robin
// over kTestChannel mocks.  The first mock expects a call, the remaining mocks
// expect no calls.  Some of these side effects can only be verified as part of
// the first mock.

class StorageStubFactory : public ::testing::Test {
 protected:
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request, storage::x_goog_api_client());
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
    CompletionQueue cq, BaseStorageStubFactory const& factory, Options opts) {
  auto credentials = google::cloud::MakeAccessTokenCredentials(
      "test-only-invalid",
      std::chrono::system_clock::now() + std::chrono::minutes(5));
  return CreateDecoratedStubs(
             std::move(cq),
             internal::MergeOptions(
                 std::move(opts),
                 Options{}
                     .set<EndpointOption>("localhost:1")
                     .set<GrpcNumChannelsOption>(kTestChannels)
                     .set<LoggingComponentsOption>({"rpc"})
                     .set<UnifiedCredentialsOption>(credentials)),
             factory)
      .second;
}

TEST_F(StorageStubFactory, ReadObject) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, ReadObject)
            .WillOnce(
                [this](auto context, Options const& options,
                       google::storage::v2::ReadObjectRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context->credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(*context,
                                   "google.storage.v2.Storage.ReadObject",
                                   request);
                  // Verify the options are passed through
                  EXPECT_THAT(options.get<UserAgentProductsOption>(),
                              Contains("test-only/1"));
                  auto stream = std::make_unique<MockObjectMediaStream>();
                  EXPECT_CALL(*stream, Read)
                      .WillOnce(Return(
                          Status(StatusCode::kUnavailable, "nothing here")));
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
  internal::AutomaticallyCreatedBackgroundThreads pool;
  auto stub = CreateTestStub(pool.cq(), factory.AsStdFunction(), {});
  auto stream =
      stub->ReadObject(std::make_shared<grpc::ClientContext>(),
                       Options{}.set<UserAgentProductsOption>({"test-only/1"}),
                       google::storage::v2::ReadObjectRequest{});
  google::storage::v2::ReadObjectResponse response;
  auto status = stream->Read(&response);
  EXPECT_TRUE(status.has_value());
  EXPECT_THAT(*status, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("ReadObject")));
}

TEST_F(StorageStubFactory, WriteObject) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, AsyncWriteObject)
            .WillOnce([this](google::cloud::CompletionQueue const&,
                             auto context, auto /*options*/) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context->credentials(), NotNull());
              // Verify the Metadata decorator is present
              IsContextMDValid(*context,
                               "google.storage.v2.Storage.WriteObject",
                               google::storage::v2::WriteObjectRequest{});
              auto stream = std::make_unique<MockAsyncInsertStream>();
              EXPECT_CALL(*stream, Start)
                  .WillOnce(Return(ByMove(make_ready_future(true))));
              EXPECT_CALL(*stream, Finish)
                  .WillOnce(Return(ByMove(make_ready_future(
                      StatusOr<google::storage::v2::WriteObjectResponse>(
                          Status(StatusCode::kUnavailable, "nothing here"))))));
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
  internal::AutomaticallyCreatedBackgroundThreads pool;
  auto stub = CreateTestStub(pool.cq(), factory.AsStdFunction(), {});
  auto stream =
      stub->AsyncWriteObject(pool.cq(), std::make_shared<grpc::ClientContext>(),
                             google::cloud::internal::MakeImmutableOptions({}));
  EXPECT_TRUE(stream->Start().get());
  auto close = stream->Finish().get();
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
            .WillOnce([this](
                          grpc::ClientContext& context, Options const&,
                          google::storage::v2::StartResumableWriteRequest const&
                              request) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              // Verify the Metadata decorator is present
              IsContextMDValid(context,
                               "google.storage.v2.Storage.StartResumableWrite",
                               request);
              return StatusOr<google::storage::v2::StartResumableWriteResponse>(
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
  internal::AutomaticallyCreatedBackgroundThreads pool;
  auto stub = CreateTestStub(pool.cq(), factory.AsStdFunction(), {});
  grpc::ClientContext context;
  auto response = stub->StartResumableWrite(context, Options{}, {});
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
            .WillOnce([this](grpc::ClientContext& context, Options const&,
                             google::storage::v2::QueryWriteStatusRequest const&
                                 request) {
              // Verify the Auth decorator is present
              EXPECT_THAT(context.credentials(), NotNull());
              // Verify the Metadata decorator is present
              IsContextMDValid(context,
                               "google.storage.v2.Storage.QueryWriteStatus",
                               request);
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
  internal::AutomaticallyCreatedBackgroundThreads pool;
  auto stub = CreateTestStub(pool.cq(), factory.AsStdFunction(), {});
  grpc::ClientContext context;
  auto response = stub->QueryWriteStatus(context, Options{}, {});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("QueryWriteStatus")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::ValidateNoPropagator;
using ::google::cloud::testing_util::ValidatePropagator;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST_F(StorageStubFactory, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, DeleteBucket)
            .WillOnce([](auto& context, auto const&, auto const&) {
              ValidatePropagator(context);
              return internal::AbortedError("fail");
            });
        return mock;
      });

  internal::AutomaticallyCreatedBackgroundThreads pool;
  auto stub = CreateTestStub(pool.cq(), factory.AsStdFunction(),
                             EnableTracing({}).set<GrpcNumChannelsOption>(1));
  grpc::ClientContext context;
  (void)stub->DeleteBucket(context, Options{}, {});

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(SpanNamed("google.storage.v2.Storage/DeleteBucket")));
}

TEST_F(StorageStubFactory, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockStorageStub>();
        EXPECT_CALL(*mock, DeleteBucket)
            .WillOnce([](auto& context, auto const&, auto const&) {
              ValidateNoPropagator(context);
              return internal::AbortedError("fail");
            });
        return mock;
      });

  internal::AutomaticallyCreatedBackgroundThreads pool;
  auto stub = CreateTestStub(pool.cq(), factory.AsStdFunction(),
                             DisableTracing({}).set<GrpcNumChannelsOption>(1));
  grpc::ClientContext context;
  (void)stub->DeleteBucket(context, Options{}, {});

  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
