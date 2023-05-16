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

#include "google/cloud/bigquery/v2/minimal/internal/project_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_log_backend.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/project_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockProjectRestStub;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

std::shared_ptr<ProjectLogging> CreateMockProjectLogging(
    std::shared_ptr<ProjectRestStub> mock) {
  return std::make_shared<ProjectLogging>(std::move(mock), TracingOptions{},
                                          std::set<std::string>{});
}

class ProjectLoggingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend_ =
        std::make_shared<bigquery_v2_minimal_testing::MockLogBackend>();
    log_backend_id_ =
        google::cloud::LogSink::Instance().AddBackend(log_backend_);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id_);
    log_backend_id_ = 0;
    log_backend_.reset();
  }

  std::shared_ptr<bigquery_v2_minimal_testing::MockLogBackend> log_backend_ =
      nullptr;
  long log_backend_id_ = 0;  // NOLINT(google-runtime-int)
};

TEST_F(ProjectLoggingClientTest, ListProjects) {
  auto mock_stub = std::make_shared<MockProjectRestStub>();

  EXPECT_CALL(*mock_stub, ListProjects)
      .WillOnce(
          [&](rest_internal::RestContext&, ListProjectsRequest const& request)
              -> StatusOr<ListProjectsResponse> {
            EXPECT_THAT(request.page_token(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload =
                bigquery_v2_minimal_testing::MakeListProjectsResponseJsonText();
            return ListProjectsResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(ListProjectsRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(max_results: 10)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(page_token: "pt-123")"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(ListProjectsResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "p-project-id")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(total_items: 1)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(next_page_token: "npt-123")"));
      });

  auto client = CreateMockProjectLogging(std::move(mock_stub));
  ListProjectsRequest request;
  request.set_max_results(10).set_page_token("pt-123");
  rest_internal::RestContext context;

  client->ListProjects(context, request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
