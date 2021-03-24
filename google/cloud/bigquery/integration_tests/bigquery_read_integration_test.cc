// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/bigquery_read_client.gcpcxx.pb.h"
#include "google/cloud/bigquery/bigquery_read_options.gcpcxx.pb.h"
#include "google/cloud/bigquery/internal/bigquery_read_stub_factory.gcpcxx.pb.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;

class BigQueryReadIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    options_.set<TracingComponentsOption>({"rpc"});
    retry_policy_ =
        absl::make_unique<BigQueryReadLimitedErrorCountRetryPolicy>(1);
    backoff_policy_ = absl::make_unique<ExponentialBackoffPolicy>(
        std::chrono::seconds(1), std::chrono::seconds(1), 2.0);
    idempotency_policy_ = MakeDefaultBigQueryReadConnectionIdempotencyPolicy();
  }

  std::vector<std::string> ClearLogLines() { return log_.ExtractLines(); }
  Options options_;
  std::unique_ptr<BigQueryReadRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  std::unique_ptr<BigQueryReadConnectionIdempotencyPolicy> idempotency_policy_;

 private:
  testing_util::ScopedLog log_;
};

std::int64_t CountRowsFromStream(
    StreamRange<::google::cloud::bigquery::storage::v1::ReadRowsResponse>&
        stream) {
  std::int64_t num_rows = 0;
  for (auto const& row : stream) {
    if (row.ok()) {
      num_rows += row->row_count();
    }
  }
  return num_rows;
}

TEST_F(BigQueryReadIntegrationTest, CreateReadSessionFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  auto response = client.CreateReadSession({}, {}, {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateReadSession")));
}

TEST_F(BigQueryReadIntegrationTest, CreateReadSessionProtoFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest request;
  auto response = client.CreateReadSession(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateReadSession")));
}

TEST_F(BigQueryReadIntegrationTest, ReadRowsFailure) {
  options_.set<TracingComponentsOption>({"rpc", "rpc-streams"});
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  auto response = client.ReadRows({}, {});
  auto begin = response.begin();
  EXPECT_FALSE(begin == response.end());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadRows")));
}

TEST_F(BigQueryReadIntegrationTest, ReadRowsProtoFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::ReadRowsRequest request;
  auto response = client.ReadRows(request);
  auto begin = response.begin();
  EXPECT_FALSE(begin == response.end());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadRows")));
}

TEST_F(BigQueryReadIntegrationTest, SplitReadStreamProtoFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::SplitReadStreamRequest request;
  auto response = client.SplitReadStream(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SplitReadStream")));
}

TEST_F(BigQueryReadIntegrationTest, CreateReadSessionSuccess) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(
      "projects/bigquery-public-data/datasets/usa_names/tables/"
      "usa_1910_current");
  auto response = client.CreateReadSession(
      "projects/cloud-cpp-testing-resources", read_session, 2);
  ASSERT_THAT(response, IsOk());
  EXPECT_GT(response->streams().size(), 0);
  EXPECT_LT(response->streams().size(), 3);
}

TEST_F(BigQueryReadIntegrationTest, CreateReadSessionProtoSuccess) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest request;
  request.set_parent("projects/cloud-cpp-testing-resources");
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(
      "projects/bigquery-public-data/datasets/usa_names/tables/"
      "usa_1910_current");
  *request.mutable_read_session() = read_session;
  auto response = client.CreateReadSession(request);
  ASSERT_THAT(response, IsOk());
  EXPECT_GT(response->streams().size(), 0);
}

TEST_F(BigQueryReadIntegrationTest, ReadRowsSuccess) {
  options_.set<TracingComponentsOption>({"rpc", "rpc-streams"});
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest
      session_request;
  session_request.set_parent("projects/cloud-cpp-testing-resources");
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(
      "projects/bigquery-public-data/datasets/usa_names/tables/"
      "usa_1910_current");
  read_session.mutable_read_options()->set_row_restriction("state = \"WA\"");
  *session_request.mutable_read_session() = read_session;
  auto session_response = client.CreateReadSession(session_request);
  ASSERT_THAT(session_response, IsOk());
  EXPECT_GT(session_response->streams().size(), 0);

  auto read_response = client.ReadRows(session_response->streams(0).name(), 0);
  auto num_rows = CountRowsFromStream(read_response);
  EXPECT_GT(num_rows, 0);
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadRows")));
}

TEST_F(BigQueryReadIntegrationTest, ReadRowsProtoSuccess) {
  options_.set<TracingComponentsOption>({"rpc", "rpc-streams"});
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));
  ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest
      session_request;
  session_request.set_parent("projects/cloud-cpp-testing-resources");
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(
      "projects/bigquery-public-data/datasets/usa_names/tables/"
      "usa_1910_current");
  read_session.mutable_read_options()->set_row_restriction("state = \"WA\"");
  *session_request.mutable_read_session() = read_session;
  auto session_response = client.CreateReadSession(session_request);
  ASSERT_THAT(session_response, IsOk());
  EXPECT_GT(session_response->streams().size(), 0);

  ::google::cloud::bigquery::storage::v1::ReadRowsRequest read_request;
  read_request.set_read_stream(session_response->streams(0).name());
  read_request.set_offset(0);
  auto read_response = client.ReadRows(read_request);
  auto num_rows = CountRowsFromStream(read_response);
  EXPECT_GT(num_rows, 0);
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadRows")));
}

TEST_F(BigQueryReadIntegrationTest, SplitReadStreamProtoSuccess) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(options_));

  // Create ReadSession with exactly 1 stream.
  ::google::cloud::bigquery::storage::v1::ReadSession read_session;
  read_session.set_data_format(
      google::cloud::bigquery::storage::v1::DataFormat::AVRO);
  read_session.set_table(
      "projects/bigquery-public-data/datasets/usa_names/tables/"
      "usa_1910_current");
  read_session.mutable_read_options()->set_row_restriction("state = \"WA\"");
  auto session_response = client.CreateReadSession(
      "projects/cloud-cpp-testing-resources", read_session, 1);
  ASSERT_THAT(session_response, IsOk());
  EXPECT_EQ(session_response->streams().size(), 1);

  // Read all rows using 1 stream.
  auto read_response = client.ReadRows(session_response->streams(0).name(), 0);
  auto num_rows = CountRowsFromStream(read_response);
  EXPECT_GT(num_rows, 0);

  // Create another ReadSession with exactly 1 stream.
  auto session_response_2 = client.CreateReadSession(
      "projects/cloud-cpp-testing-resources", read_session, 1);
  ASSERT_THAT(session_response_2, IsOk());
  EXPECT_EQ(session_response_2->streams().size(), 1);

  // Split the stream in half.
  ::google::cloud::bigquery::storage::v1::SplitReadStreamRequest split_request;
  split_request.set_name(session_response_2->streams(0).name());
  split_request.set_fraction(0.5);
  auto split_response = client.SplitReadStream(split_request);
  ASSERT_THAT(split_response, IsOk());

  auto primary_read_response =
      client.ReadRows(split_response->primary_stream().name(), 0);
  auto primary_num_rows = CountRowsFromStream(primary_read_response);

  auto remainder_read_response =
      client.ReadRows(split_response->remainder_stream().name(), 0);
  auto remainder_num_rows = CountRowsFromStream(remainder_read_response);
  EXPECT_EQ(num_rows, primary_num_rows + remainder_num_rows);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SplitReadStream")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
