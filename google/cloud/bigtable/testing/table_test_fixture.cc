// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/throw_delegate.h"
#include <google/protobuf/text_format.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

char const TableTestFixture::kProjectId[] = "foo-project";
char const TableTestFixture::kInstanceId[] = "bar-instance";
char const TableTestFixture::kTableId[] = "baz-table";

// These are hardcoded, and not computed, because we want to test the
// computation.
char const TableTestFixture::kInstanceName[] =
    "projects/foo-project/instances/bar-instance";
char const TableTestFixture::kTableName[] =
    "projects/foo-project/instances/bar-instance/tables/baz-table";

google::bigtable::v2::ReadRowsResponse ReadRowsResponseFromString(
    std::string const& repr) {
  google::bigtable::v2::ReadRowsResponse response;
  if (!google::protobuf::TextFormat::ParseFromString(repr, &response)) {
    google::cloud::internal::ThrowRuntimeError("Failed to parse " + repr);
  }
  return response;
}

std::shared_ptr<MockDataClient> TableTestFixture::SetupMockClient() {
  auto client = std::make_shared<MockDataClient>(
      ClientOptions().DisableBackgroundThreads(cq_));
  EXPECT_CALL(*client, project_id())
      .WillRepeatedly(::testing::ReturnRef(project_id_));
  EXPECT_CALL(*client, instance_id())
      .WillRepeatedly(::testing::ReturnRef(instance_id_));
  return client;
}

TableTestFixture::TableTestFixture(CompletionQueue cq)
    : cq_impl_(std::dynamic_pointer_cast<
               ::google::cloud::testing_util::FakeCompletionQueueImpl>(
          google::cloud::internal::GetCompletionQueueImpl(cq))),
      cq_(cq),
      client_(SetupMockClient()) {}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
