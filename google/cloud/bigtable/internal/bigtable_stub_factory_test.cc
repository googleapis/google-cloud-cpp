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

#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockReadRowsStream;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Return;

// The point of these tests is to verify that the `CreateBigtableStub` factory
// function injects the right decorators. We do this by observing the
// side effects of these decorators. All the tests have nearly identical
// structure. They create a fully decorated stub, configured to round-robin
// over kTestChannel mocks.  The first mock expects a call, the remaining mocks
// expect no calls.  Some of these side effects can only be verified as part of
// the first mock.

class BigtableStubFactory : public ::testing::Test {
 protected:
  template <typename Request>
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        Request const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request,
        google::cloud::internal::ApiClientHeader("generator"));
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kTestChannels = 3;

using MockFactory = ::testing::MockFunction<std::shared_ptr<BigtableStub>(
    std::shared_ptr<grpc::Channel>)>;
static_assert(std::is_same<decltype(MockFactory{}.AsStdFunction()),
                           BaseBigtableStubFactory>::value,
              "Mismatched mock factory type");

std::shared_ptr<BigtableStub> CreateTestStub(
    CompletionQueue cq, BaseBigtableStubFactory const& factory) {
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

TEST_F(BigtableStubFactory, ReadRows) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, ReadRows)
            .WillOnce(
                [this](std::unique_ptr<grpc::ClientContext> context,
                       google::bigtable::v2::ReadRowsRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context->credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(*context,
                                   "google.bigtable.v2.Bigtable.ReadRows",
                                   request);
                  auto stream = absl::make_unique<MockReadRowsStream>();
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
        return std::make_shared<MockBigtableStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  google::bigtable::v2::ReadRowsRequest req;
  req.set_table_name(
      "projects/the-project/instances/the-instance/tables/the-table");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto stream = stub->ReadRows(absl::make_unique<grpc::ClientContext>(), req);
  auto read = stream->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(read));
  EXPECT_THAT(absl::get<Status>(read), StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("ReadRows")));
}

TEST_F(BigtableStubFactory, MutateRow) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, MutateRow)
            .WillOnce(
                [this](grpc::ClientContext& context,
                       google::bigtable::v2::MutateRowRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context.credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(context,
                                   "google.bigtable.v2.Bigtable.MutateRow",
                                   request);
                  return StatusOr<google::bigtable::v2::MutateRowResponse>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockBigtableStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest req;
  req.set_table_name(
      "projects/the-project/instances/the-instance/tables/the-table");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response = stub->MutateRow(context, req);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("MutateRow")));
}

TEST_F(BigtableStubFactory, AsyncReadRows) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, AsyncReadRows)
            .WillOnce(
                [this](CompletionQueue const&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::bigtable::v2::ReadRowsRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context->credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(*context,
                                   "google.bigtable.v2.Bigtable.ReadRows",
                                   request);
                  using ErrorStream =
                      ::google::cloud::internal::AsyncStreamingReadRpcError<
                          google::bigtable::v2::ReadRowsResponse>;
                  return absl::make_unique<ErrorStream>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockBigtableStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  google::bigtable::v2::ReadRowsRequest req;
  req.set_table_name(
      "projects/the-project/instances/the-instance/tables/the-table");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto stream =
      stub->AsyncReadRows(cq, absl::make_unique<grpc::ClientContext>(), req);
  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("AsyncReadRows")));
}

TEST_F(BigtableStubFactory, AsyncMutateRow) {
  ::testing::InSequence sequence;
  MockFactory factory;
  EXPECT_CALL(factory, Call)
      .WillOnce([this](std::shared_ptr<grpc::Channel> const&) {
        auto mock = std::make_shared<MockBigtableStub>();
        EXPECT_CALL(*mock, AsyncMutateRow)
            .WillOnce(
                [this](CompletionQueue&,
                       std::unique_ptr<grpc::ClientContext> context,
                       google::bigtable::v2::MutateRowRequest const& request) {
                  // Verify the Auth decorator is present
                  EXPECT_THAT(context->credentials(), NotNull());
                  // Verify the Metadata decorator is present
                  IsContextMDValid(*context,
                                   "google.bigtable.v2.Bigtable.MutateRow",
                                   request);
                  return make_ready_future<
                      StatusOr<google::bigtable::v2::MutateRowResponse>>(
                      Status(StatusCode::kUnavailable, "nothing here"));
                });
        return mock;
      });
  EXPECT_CALL(factory, Call)
      .Times(kTestChannels - 1)
      .WillRepeatedly([](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<MockBigtableStub>();
      });

  ScopedLog log;
  CompletionQueue cq;
  google::bigtable::v2::MutateRowRequest req;
  req.set_table_name(
      "projects/the-project/instances/the-instance/tables/the-table");
  auto stub = CreateTestStub(cq, factory.AsStdFunction());
  auto response =
      stub->AsyncMutateRow(cq, absl::make_unique<grpc::ClientContext>(), req);
  EXPECT_THAT(response.get(), StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("AsyncMutateRow")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
