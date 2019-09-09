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

#include "google/cloud/spanner/instance_admin_connection.h"
#include "google/cloud/spanner/testing/matchers.h"
#include "google/cloud/spanner/testing/mock_instance_admin_stub.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::spanner_testing::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace gcsa = ::google::spanner::admin::instance::v1;

std::shared_ptr<InstanceAdminConnection> MakeTestConnection(
    std::shared_ptr<spanner_testing::MockInstanceAdminStub> mock) {
  return internal::MakeInstanceAdminConnection(
      std::move(mock), ConnectionOptions(),
      LimitedErrorCountRetryPolicy(/*maximum_failures=*/2).clone(),
      ExponentialBackoffPolicy(/*initial_delay=*/std::chrono::microseconds(1),
                               /*maximum_delay=*/std::chrono::microseconds(1),
                               /*scaling=*/2.0)
          .clone());
}

TEST(InstanceAdminConnectionTest, GetInstance_Success) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  gcsa::Instance expected_instance;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        name: "projects/test-project/instances/test-instance"
        config: "test-config"
        display_name: "test display name"
        node_count: 7
        state: CREATING
      )pb",
      &expected_instance));

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstance(_, _))
      .WillOnce(
          Invoke([&expected_name](grpc::ClientContext&,
                                  gcsa::GetInstanceRequest const& request) {
            EXPECT_EQ(expected_name, request.name());
            return Status(StatusCode::kUnavailable, "try-again");
          }))
      .WillOnce(Invoke(
          [&expected_name, &expected_instance](
              grpc::ClientContext&, gcsa::GetInstanceRequest const& request) {
            EXPECT_EQ(expected_name, request.name());
            return expected_instance;
          }));

  auto conn = MakeTestConnection(mock);
  auto actual = conn->GetInstance({expected_name});
  EXPECT_THAT(*actual, IsProtoEqual(expected_instance));
}

TEST(InstanceAdminConnectionTest, GetInstance_PermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstance(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeTestConnection(mock);
  auto actual = conn->GetInstance({"test-name"});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminConnectionTest, GetInstance_TooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstance(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeTestConnection(mock);
  auto actual = conn->GetInstance({"test-name"});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(InstanceAdminConnectionTest, ListInstances_Success) {
  std::string const expected_parent = "projects/test-project";

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstances(_, _))
      .WillOnce(Invoke(
          [&](grpc::ClientContext&, gcsa::ListInstancesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("labels.test-key:test-value", request.filter());
            EXPECT_TRUE(request.page_token().empty());
            return Status(StatusCode::kUnavailable, "try-again");
          }))
      .WillOnce(Invoke(
          [&](grpc::ClientContext&, gcsa::ListInstancesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("labels.test-key:test-value", request.filter());
            EXPECT_TRUE(request.page_token().empty());

            gcsa::ListInstancesResponse response;
            response.set_next_page_token("p1");
            response.add_instances()->set_name("i1");
            response.add_instances()->set_name("i2");
            return response;
          }))
      .WillOnce(Invoke(
          [&](grpc::ClientContext&, gcsa::ListInstancesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("labels.test-key:test-value", request.filter());
            EXPECT_EQ("p1", request.page_token());

            gcsa::ListInstancesResponse response;
            response.clear_next_page_token();
            response.add_instances()->set_name("i3");
            return response;
          }));

  auto conn = MakeTestConnection(mock);
  std::vector<std::string> actual_names;
  for (auto instance :
       conn->ListInstances({"test-project", "labels.test-key:test-value"})) {
    ASSERT_STATUS_OK(instance);
    actual_names.push_back(instance->name());
  }
  EXPECT_THAT(actual_names, ::testing::ElementsAre("i1", "i2", "i3"));
}

TEST(InstanceAdminConnectionTest, ListInstances_PermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstances(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeTestConnection(mock);
  auto range = conn->ListInstances({"test-project", ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(InstanceAdminConnectionTest, ListInstances_TooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstances(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeTestConnection(mock);
  auto range = conn->ListInstances({"test-project", ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kUnavailable, begin->status().code());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
