// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/sql/v1/sql_instances_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace sql_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

class SqlIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
  }

  std::string project_id_;
};

TEST_F(SqlIntegrationTest, PaginatedList) {
  namespace sql = ::google::cloud::sql_v1;
  auto client = sql::SqlInstancesServiceClient(
      sql::MakeSqlInstancesServiceConnectionRest());

  google::cloud::sql::v1::SqlInstancesListRequest request;
  request.set_project(project_id_);
  for (auto database : client.List(request)) {
    ASSERT_STATUS_OK(database);
    EXPECT_THAT(database->name(), Not(IsEmpty()));
  }
}

// Test added for issue #12112.
// This test was added to verify that the correct JSON request body is being
// sent for rpcs that specify a field of the request as the body.
//
// message SqlInstancesCreateEphemeralCertRequest {
//   // Cloud SQL instance ID. This does not include the project ID.
//  string instance = 1;
//  // Project ID of the Cloud SQL project.
//  string project = 2;
//  SslCertsCreateEphemeralRequest body = 100;
//}
//
//  rpc CreateEphemeral(SqlInstancesCreateEphemeralCertRequest)
//      returns (SslCert) {
//    option (google.api.http) = {
//      post: "/v1/projects/{project}/instances/{instance}/createEphemeral"
//      body: "body"
//    };
//  }
//
// When the `public_key` field was not present in the JSON request body, the
// server returned the error:
//   INVALID_ARGUMENT: Error in non-idempotent operation CreateEphemeral:
//   Missing parameter: public_key.
// While the request is not successful due to other reasons, depending on
// the environment/permissions, the public_key field is being read
// successfully.
TEST_F(SqlIntegrationTest, CreateEphemeral) {
  namespace sql = ::google::cloud::sql_v1;
  auto client = sql::SqlInstancesServiceClient(
      sql::MakeSqlInstancesServiceConnectionRest());

  google::cloud::sql::v1::SqlInstancesListRequest list_request;
  list_request.set_project(project_id_);
  auto instances = client.List(list_request);
  ASSERT_NE(instances.begin(), instances.end());
  ASSERT_STATUS_OK(*instances.begin());

  google::cloud::sql::v1::SqlInstancesCreateEphemeralCertRequest request;
  request.set_project(project_id_);
  request.set_instance((*instances.begin())->name());
  request.mutable_body()->set_public_key("THE_PUBLIC_KEY");

  auto status = [&]() {
    // All the RPCs that could repro #12112 are non-idempotent. So we put a
    // little manual retry loop around the test.
    auto const attempt_delay = std::chrono::seconds(30);
    Status status;
    for (int attempt = 0; attempt != 3; ++attempt) {
      if (attempt != 0) std::this_thread::sleep_for(attempt_delay);

      status = client.CreateEphemeral(request).status();
      // This is a transient error, just retry.
      if (status.code() != StatusCode::kUnavailable) break;
    }
    return status;
  }();

  EXPECT_THAT(
      status,
      AnyOf(
          StatusIs(StatusCode::kInvalidArgument,
                   HasSubstr("Provided public key was in an invalid or "
                             "unsupported format")),
          StatusIs(
              StatusCode::kPermissionDenied,
              HasSubstr("The client is not authorized to make this request"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace sql_v1
}  // namespace cloud
}  // namespace google
