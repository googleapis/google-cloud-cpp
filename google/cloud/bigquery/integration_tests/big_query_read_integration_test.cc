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

#include "google/cloud/bigquery/big_query_read_client.gcpcxx.pb.h"
#include "google/cloud/bigquery/internal/big_query_read_stub_factory.gcpcxx.pb.h"
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
    ::google::cloud::testing_util::IntegrationTest::SetUp();
    connection_options_.enable_tracing("rpc");
    retry_policy_ =
        absl::make_unique<BigQueryReadLimitedErrorCountRetryPolicy>(1);
    backoff_policy_ = absl::make_unique<ExponentialBackoffPolicy>(
        std::chrono::seconds(1), std::chrono::seconds(1), 2.0);
    idempotency_policy_ = MakeDefaultBigQueryReadConnectionIdempotencyPolicy();
  }

  std::vector<std::string> ClearLogLines() { return log_.ExtractLines(); }
  BigQueryReadConnectionOptions connection_options_;
  std::unique_ptr<BigQueryReadRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  std::unique_ptr<BigQueryReadConnectionIdempotencyPolicy> idempotency_policy_;

 private:
  testing_util::ScopedLog log_;
};

TEST_F(BigQueryReadIntegrationTest, CreateReadSessionFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      idempotency_policy_->clone()));
  auto response = client.CreateReadSession({}, {}, {});
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateReadSession")));
}

TEST_F(BigQueryReadIntegrationTest, CreateReadSessionProtoFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      idempotency_policy_->clone()));
  ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest request;
  auto response = client.CreateReadSession(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateReadSession")));
}

TEST_F(BigQueryReadIntegrationTest, ReadRowsFailure) {
  connection_options_.enable_tracing("rpc-streams");
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      idempotency_policy_->clone()));
  auto response = client.ReadRows({}, {});
  auto begin = response.begin();
  EXPECT_FALSE(begin == response.end());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadRows")));
}

TEST_F(BigQueryReadIntegrationTest, ReadRowsProtoFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      idempotency_policy_->clone()));
  ::google::cloud::bigquery::storage::v1::ReadRowsRequest request;
  auto response = client.ReadRows(request);
  auto begin = response.begin();
  EXPECT_FALSE(begin == response.end());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadRows")));
}

TEST_F(BigQueryReadIntegrationTest, SplitReadStreamProtoFailure) {
  auto client = BigQueryReadClient(MakeBigQueryReadConnection(
      connection_options_, retry_policy_->clone(), backoff_policy_->clone(),
      idempotency_policy_->clone()));
  ::google::cloud::bigquery::storage::v1::SplitReadStreamRequest request;
  auto response = client.SplitReadStream(request);
  EXPECT_THAT(response, Not(IsOk()));
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("SplitReadStream")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
