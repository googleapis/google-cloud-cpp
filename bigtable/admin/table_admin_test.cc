//   Copyright 2017 Google Inc.
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "bigtable/admin/table_admin.h"

#include <gmock/gmock.h>

namespace {
class MockAdminClient : public bigtable::AdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD1(OnFailure, void(grpc::Status const& status));
  MOCK_METHOD0(
      table_admin,
      ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface&());
};
}  // namespace

/// @test Verify basic functionality in the bigtable::TableAdmin class.
TEST(TableAdmin, Simple) {
  using namespace ::testing;
  std::string const project_id = "the-project";

  auto client = std::make_shared<MockAdminClient>();
  EXPECT_CALL(*client, project())
      .WillRepeatedly(ReturnRef(project_id));
  bigtable::TableAdmin tested(client, "the-instance");
  EXPECT_EQ("the-instance", tested.instance_id());
  EXPECT_EQ("/projects/the-project/instances/the-instance",
            tested.instance_name());
}
