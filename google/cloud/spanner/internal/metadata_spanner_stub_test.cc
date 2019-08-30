// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/metadata_spanner_stub.h"
#include "google/cloud/spanner/internal/compiler_info.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/spanner/testing/validate_metadata.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
namespace spanner_proto = google::spanner::v1;

// This ugly macro and the supporting template member function refactor most
// of this test to one-liners.
#define SESSION_TEST(X, Y) \
  ExpectSession(EXPECT_CALL(*mock_, X(_, _)), Y(), #X, &MetadataSpannerStub::X)

class MetadataSpannerStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockSpannerStub>();
    MetadataSpannerStub stub(mock_);
    expected_api_client_header_ = stub.api_client_header();
  }

  void TearDown() override {}

  Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  template <typename T>
  void ExpectTransientError(StatusOr<T> const& status) {
    EXPECT_EQ(TransientError(), status.status());
  }

  void ExpectTransientError(Status const& status) {
    EXPECT_EQ(TransientError(), status);
  }

  template <typename MockCall, typename Request, typename MemberFunction>
  void ExpectSession(MockCall& call, Request const&,
                     std::string const& rpc_name,
                     MemberFunction member_function) {
    call.WillOnce(
        Invoke([this, rpc_name](grpc::ClientContext& context, Request const&) {
          EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
              context, "google.spanner.v1.Spanner." + rpc_name,
              expected_api_client_header_));
          return TransientError();
        }));

    MetadataSpannerStub stub(mock_);
    grpc::ClientContext context;
    Request request;
    request.set_session(
        "projects/test-project-id/"
        "instances/test-instance-id/"
        "databases/test-database-id/"
        "sessions/test-session-id");
    auto result = (stub.*member_function)(context, request);
    ExpectTransientError(result);
  }

  std::shared_ptr<spanner_testing::MockSpannerStub> mock_;
  std::string expected_api_client_header_;
};

TEST_F(MetadataSpannerStubTest, ApiClientHeader) {
  MetadataSpannerStub stub(mock_);

  EXPECT_THAT(stub.api_client_header(), HasSubstr("gccl/" + VersionString()));
  EXPECT_THAT(stub.api_client_header(),
              HasSubstr("gl-cpp/" + CompilerId() + "-" + CompilerVersion() +
                        "-" + CompilerFeatures() + "-" + LanguageVersion()));
}

TEST_F(MetadataSpannerStubTest, CreateSession) {
  EXPECT_CALL(*mock_, CreateSession(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              spanner_proto::CreateSessionRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.spanner.v1.Spanner.CreateSession",
            expected_api_client_header_));
        return TransientError();
      }));

  MetadataSpannerStub stub(mock_);
  grpc::ClientContext context;
  spanner_proto::CreateSessionRequest request;
  request.set_database(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database-id");
  auto status = stub.CreateSession(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataSpannerStubTest, GetSession) {
  EXPECT_CALL(*mock_, GetSession(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              spanner_proto::GetSessionRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.spanner.v1.Spanner.GetSession",
            expected_api_client_header_));
        return TransientError();
      }));

  MetadataSpannerStub stub(mock_);
  grpc::ClientContext context;
  spanner_proto::GetSessionRequest request;
  request.set_name(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database-id/"
      "sessions/test-session-id");
  auto status = stub.GetSession(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataSpannerStubTest, ListSessions) {
  EXPECT_CALL(*mock_, ListSessions(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              spanner_proto::ListSessionsRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.spanner.v1.Spanner.ListSessions",
            expected_api_client_header_));
        return TransientError();
      }));

  MetadataSpannerStub stub(mock_);
  grpc::ClientContext context;
  spanner_proto::ListSessionsRequest request;
  request.set_database(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database-id");
  auto status = stub.ListSessions(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataSpannerStubTest, DeleteSession) {
  EXPECT_CALL(*mock_, DeleteSession(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              spanner_proto::DeleteSessionRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.spanner.v1.Spanner.DeleteSession",
            expected_api_client_header_));
        return TransientError();
      }));

  MetadataSpannerStub stub(mock_);
  grpc::ClientContext context;
  spanner_proto::DeleteSessionRequest request;
  request.set_name(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database-id/"
      "sessions/test-session-id");
  auto status = stub.DeleteSession(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(MetadataSpannerStubTest, ExecuteSql) {
  SESSION_TEST(ExecuteSql, spanner_proto::ExecuteSqlRequest);
}

TEST_F(MetadataSpannerStubTest, ExecuteStreamingSql) {
  EXPECT_CALL(*mock_, ExecuteStreamingSql(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              spanner_proto::ExecuteSqlRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.spanner.v1.Spanner.ExecuteStreamingSql",
            expected_api_client_header_));
        return std::unique_ptr<
            grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>{};
      }));

  MetadataSpannerStub stub(mock_);
  grpc::ClientContext context;
  spanner_proto::ExecuteSqlRequest request;
  request.set_session(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database-id/"
      "sessions/test-session-id");
  auto result = stub.ExecuteStreamingSql(context, request);
  EXPECT_FALSE(result);
}

TEST_F(MetadataSpannerStubTest, ExecuteBatchDml) {
  SESSION_TEST(ExecuteBatchDml, spanner_proto::ExecuteBatchDmlRequest);
}

TEST_F(MetadataSpannerStubTest, Read) {
  SESSION_TEST(Read, spanner_proto::ReadRequest);
}

TEST_F(MetadataSpannerStubTest, StreamingRead) {
  EXPECT_CALL(*mock_, StreamingRead(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              spanner_proto::ReadRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.spanner.v1.Spanner.StreamingRead",
            expected_api_client_header_));
        return std::unique_ptr<
            grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>{};
      }));

  MetadataSpannerStub stub(mock_);
  grpc::ClientContext context;
  spanner_proto::ReadRequest request;
  request.set_session(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database-id/"
      "sessions/test-session-id");
  auto result = stub.StreamingRead(context, request);
  EXPECT_FALSE(result);
}

TEST_F(MetadataSpannerStubTest, BeginTransaction) {
  SESSION_TEST(BeginTransaction, spanner_proto::BeginTransactionRequest);
}

TEST_F(MetadataSpannerStubTest, Commit) {
  SESSION_TEST(Commit, spanner_proto::CommitRequest);
}

TEST_F(MetadataSpannerStubTest, Rollback) {
  SESSION_TEST(Rollback, spanner_proto::RollbackRequest);
}

TEST_F(MetadataSpannerStubTest, PartitionQuery) {
  SESSION_TEST(PartitionQuery, spanner_proto::PartitionQueryRequest);
}

TEST_F(MetadataSpannerStubTest, PartitionRead) {
  SESSION_TEST(PartitionRead, spanner_proto::PartitionReadRequest);
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
