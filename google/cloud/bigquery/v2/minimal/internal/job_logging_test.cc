// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/internal/job_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_log_backend.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_job_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockBigQueryJobRestStub;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

std::shared_ptr<BigQueryJobLogging> CreateMockJobLogging(
    std::shared_ptr<BigQueryJobRestStub> mock) {
  return std::make_shared<BigQueryJobLogging>(std::move(mock), TracingOptions{},
                                              std::set<std::string>{});
}

class JobLoggingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend_ =
        std::make_shared<bigquery_v2_minimal_testing::MockJobLogBackend>();
    log_backend_id_ =
        google::cloud::LogSink::Instance().AddBackend(log_backend_);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id_);
    log_backend_id_ = 0;
    log_backend_.reset();
  }

  std::shared_ptr<bigquery_v2_minimal_testing::MockJobLogBackend> log_backend_ =
      nullptr;
  long log_backend_id_ = 0;  // NOLINT(google-runtime-int)
};

TEST_F(JobLoggingClientTest, GetJob) {
  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"kind": "jkind",
          "etag": "jtag",
          "id": "j123",
          "self_link": "jselfLink",
          "user_email": "juserEmail",
          "status": {"state": "DONE"},
          "reference": {"project_id": "p123", "job_id": "j123"},
          "configuration": {
            "job_type": "QUERY",
            "query_config": {"query": "select 1;"}
          }})";

  EXPECT_CALL(*mock_stub, GetJob)
      .WillOnce([&](rest_internal::RestContext&,
                    GetJobRequest const& request) -> StatusOr<GetJobResponse> {
        EXPECT_THAT(request.project_id(), Not(IsEmpty()));
        EXPECT_THAT(request.job_id(), Not(IsEmpty()));
        BigQueryHttpResponse http_response;
        http_response.payload = kExpectedPayload;
        return GetJobResponse::BuildFromHttpResponse(std::move(http_response));
      });

  // Not checking all fields exhaustively. Just that request and response are
  // being logged.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(GetJobRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(job_id: "j123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p123")"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(GetJobResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(status: "DONE")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(id: "j123")"));
      });

  auto client = CreateMockJobLogging(std::move(mock_stub));
  GetJobRequest request("p123", "j123");
  rest_internal::RestContext context;

  client->GetJob(context, request);
}

TEST_F(JobLoggingClientTest, ListJobs) {
  auto mock_stub = std::make_shared<MockBigQueryJobRestStub>();
  auto constexpr kExpectedPayload =
      R"({"etag": "tag-1",
          "kind": "kind-1",
          "next_page_token": "npt-123",
          "jobs": [
              {
                "id": "1",
                "kind": "kind-2",
                "reference": {"project_id": "p123", "job_id": "j123"},
                "state": "DONE",
                "configuration": {
                   "job_type": "QUERY",
                   "query_config": {"query": "select 1;"}
                },
                "status": {"state": "DONE"},
                "user_email": "user-email",
                "principal_subject": "principal-subj"
              }
  ]})";

  EXPECT_CALL(*mock_stub, ListJobs)
      .WillOnce(
          [&](rest_internal::RestContext&,
              ListJobsRequest const& request) -> StatusOr<ListJobsResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload = kExpectedPayload;
            return ListJobsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(ListJobsRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(all_users: false)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(max_results: 0)"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(ListJobsResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(id: "1")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(state: "DONE")"));
      });

  auto client = CreateMockJobLogging(std::move(mock_stub));
  ListJobsRequest request("p123");
  rest_internal::RestContext context;

  client->ListJobs(context, request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
