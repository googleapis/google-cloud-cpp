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

namespace google {
namespace cloud {
namespace sql_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

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
  auto client =
      sql::SqlInstancesServiceClient(google::cloud::ExperimentalTag{},
                                     sql::MakeSqlInstancesServiceConnectionRest(
                                         google::cloud::ExperimentalTag{}));

  google::cloud::sql::v1::SqlInstancesListRequest request;
  request.set_project(project_id_);
  for (auto database : client.List(request)) {
    ASSERT_STATUS_OK(database);
    EXPECT_THAT(database->name(), Not(IsEmpty()));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace sql_v1
}  // namespace cloud
}  // namespace google
