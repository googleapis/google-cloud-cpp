// Copyright 2020 Google Inc.
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

#include "google/cloud/bigtable/internal/logging_data_client.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/testing/mock_data_client.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

namespace btproto = ::google::bigtable::v2;

class LoggingDataClientTest : public ::testing::Test {
 protected:
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
};

TEST_F(LoggingDataClientTest, MutateRow) {
  auto mock = std::make_shared<testing::MockDataClient>();

  EXPECT_CALL(*mock, MutateRow).WillOnce(Return(grpc::Status()));

  internal::LoggingDataClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btproto::MutateRowRequest request;
  btproto::MutateRowResponse response;

  auto status = stub.MutateRow(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("MutateRow")));
}

TEST_F(LoggingDataClientTest, CheckAndMutateRow) {
  auto mock = std::make_shared<testing::MockDataClient>();

  EXPECT_CALL(*mock, CheckAndMutateRow).WillOnce(Return(grpc::Status()));

  internal::LoggingDataClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btproto::CheckAndMutateRowRequest request;
  btproto::CheckAndMutateRowResponse response;

  auto status = stub.CheckAndMutateRow(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CheckAndMutateRow")));
}

TEST_F(LoggingDataClientTest, ReadModifyWriteRow) {
  auto mock = std::make_shared<testing::MockDataClient>();

  EXPECT_CALL(*mock, ReadModifyWriteRow).WillOnce(Return(grpc::Status()));

  internal::LoggingDataClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btproto::ReadModifyWriteRowRequest request;
  btproto::ReadModifyWriteRowResponse response;

  auto status = stub.ReadModifyWriteRow(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ReadModifyWriteRow")));
}

TEST_F(LoggingDataClientTest, ReadRows) {
  auto mock = std::make_shared<testing::MockDataClient>();

  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](grpc::ClientContext*, btproto::ReadRowsRequest const&) {
        return std::unique_ptr<
            grpc::ClientReaderInterface<btproto::ReadRowsResponse>>{};
      });

  internal::LoggingDataClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btproto::ReadRowsRequest request;

  stub.ReadRows(&context, request);

  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ReadRows")));
}

TEST_F(LoggingDataClientTest, SampleRowKeys) {
  auto mock = std::make_shared<testing::MockDataClient>();

  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce([](grpc::ClientContext*, btproto::SampleRowKeysRequest const&) {
        return std::unique_ptr<
            grpc::ClientReaderInterface<btproto::SampleRowKeysResponse>>{};
      });

  internal::LoggingDataClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btproto::SampleRowKeysRequest request;

  stub.SampleRowKeys(&context, request);

  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("SampleRowKeys")));
}

TEST_F(LoggingDataClientTest, MutateRows) {
  auto mock = std::make_shared<testing::MockDataClient>();

  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
        return std::unique_ptr<
            grpc::ClientReaderInterface<btproto::MutateRowsResponse>>{};
      });

  internal::LoggingDataClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btproto::MutateRowsRequest request;

  stub.MutateRows(&context, request);

  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("MutateRows")));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
