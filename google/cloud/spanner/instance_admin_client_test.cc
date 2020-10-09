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

#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/mocks/mock_instance_admin_connection.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using spanner_mocks::MockInstanceAdminConnection;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
namespace gcsa = ::google::spanner::admin::instance::v1;

TEST(InstanceAdminClientTest, CopyAndMove) {
  auto conn1 = std::make_shared<MockInstanceAdminConnection>();
  auto conn2 = std::make_shared<MockInstanceAdminConnection>();

  InstanceAdminClient c1(conn1);
  InstanceAdminClient c2(conn2);
  EXPECT_NE(c1, c2);

  // Copy construction
  InstanceAdminClient c3 = c1;
  EXPECT_EQ(c3, c1);

  // Copy assignment
  c3 = c2;
  EXPECT_EQ(c3, c2);

  // Move construction
  InstanceAdminClient c4 = std::move(c3);
  EXPECT_EQ(c4, c2);

  // Move assignment
  c1 = std::move(c4);
  EXPECT_EQ(c1, c2);
}

TEST(InstanceAdminClientTest, GetInstance) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetInstance(_))
      .WillOnce([](InstanceAdminConnection::GetInstanceParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual = client.GetInstance(Instance("test-project", "test-instance"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, GetInstanceConfig) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetInstanceConfig(_))
      .WillOnce([](InstanceAdminConnection::GetInstanceConfigParams const& p) {
        EXPECT_EQ("projects/test-project/instanceConfigs/test-config",
                  p.instance_config_name);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual = client.GetInstanceConfig(
      "projects/test-project/instanceConfigs/test-config");
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, ListInstanceConfigs) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, ListInstanceConfigs(_))
      .WillOnce(
          [](InstanceAdminConnection::ListInstanceConfigsParams const& p) {
            EXPECT_EQ("test-project", p.project_id);
            return ListInstanceConfigsRange(
                gcsa::ListInstanceConfigsRequest{},
                [](gcsa::ListInstanceConfigsRequest const&) {
                  return StatusOr<gcsa::ListInstanceConfigsResponse>(
                      Status(StatusCode::kPermissionDenied, "uh-oh"));
                },
                [](gcsa::ListInstanceConfigsResponse const&) {
                  return std::vector<gcsa::InstanceConfig>{};
                });
          });

  InstanceAdminClient client(mock);
  auto range = client.ListInstanceConfigs("test-project");
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, ListInstances) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();

  EXPECT_CALL(*mock, ListInstances(_))
      .WillOnce([](InstanceAdminConnection::ListInstancesParams const& p) {
        EXPECT_EQ("test-project", p.project_id);
        EXPECT_EQ("labels.test-key:test-value", p.filter);

        return ListInstancesRange(
            gcsa::ListInstancesRequest{},
            [](gcsa::ListInstancesRequest const&) {
              return StatusOr<gcsa::ListInstancesResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](gcsa::ListInstancesResponse const&) {
              return std::vector<gcsa::Instance>{};
            });
      });

  InstanceAdminClient client(mock);
  auto range =
      client.ListInstances("test-project", "labels.test-key:test-value");
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, GetIamPolicy) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual = client.GetIamPolicy(Instance("test-project", "test-instance"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, SetIamPolicy) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, SetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::SetIamPolicyParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual = client.SetIamPolicy(Instance("test-project", "test-instance"),
                                    google::iam::v1::Policy{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, SetIamPolicyOccGetFailure) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual =
      client.SetIamPolicy(Instance("test-project", "test-instance"),
                          [](google::iam::v1::Policy const&) {
                            return absl::optional<google::iam::v1::Policy>{};
                          });
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(InstanceAdminClientTest, SetIamPolicyOccNoUpdates) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        google::iam::v1::Policy r;
        r.set_etag("test-etag");
        return r;
      });
  EXPECT_CALL(*mock, SetIamPolicy(_)).Times(0);

  InstanceAdminClient client(mock);
  auto actual =
      client.SetIamPolicy(Instance("test-project", "test-instance"),
                          [](google::iam::v1::Policy const& p) {
                            EXPECT_EQ("test-etag", p.etag());
                            return absl::optional<google::iam::v1::Policy>{};
                          });
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("test-etag", actual->etag());
}

std::unique_ptr<TransactionRerunPolicy> RerunPolicyForTesting() {
  return LimitedErrorCountTransactionRerunPolicy(/*maximum_failures=*/3)
      .clone();
}

std::unique_ptr<BackoffPolicy> BackoffPolicyForTesting() {
  return ExponentialBackoffPolicy(
             /*initial_delay=*/std::chrono::microseconds(1),
             /*maximum_delay=*/std::chrono::microseconds(1), /*scaling=*/2.0)
      .clone();
}

TEST(InstanceAdminClientTest, SetIamPolicyOccRetryAborted) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        google::iam::v1::Policy r;
        r.set_etag("test-etag-1");
        return r;
      })
      .WillOnce([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        google::iam::v1::Policy r;
        r.set_etag("test-etag-2");
        return r;
      });
  EXPECT_CALL(*mock, SetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::SetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        EXPECT_EQ("test-etag-1", p.policy.etag());
        return Status(StatusCode::kAborted, "aborted");
      })
      .WillOnce([](InstanceAdminConnection::SetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        EXPECT_EQ("test-etag-2", p.policy.etag());
        google::iam::v1::Policy r;
        r.set_etag("test-etag-3");
        return r;
      });

  InstanceAdminClient client(mock);
  int counter = 0;
  auto actual = client.SetIamPolicy(
      Instance("test-project", "test-instance"),
      [&counter](google::iam::v1::Policy p) {
        EXPECT_EQ("test-etag-" + std::to_string(++counter), p.etag());
        return p;
      },
      RerunPolicyForTesting(), BackoffPolicyForTesting());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("test-etag-3", actual->etag());
}

TEST(InstanceAdminClientTest, SetIamPolicyOccRetryAbortedTooManyFailures) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillRepeatedly([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        google::iam::v1::Policy r;
        r.set_etag("test-etag-1");
        return r;
      });
  EXPECT_CALL(*mock, SetIamPolicy(_))
      .Times(AtLeast(2))
      .WillRepeatedly([](InstanceAdminConnection::SetIamPolicyParams const& p) {
        EXPECT_THAT(p.instance_name, HasSubstr("test-project"));
        EXPECT_THAT(p.instance_name, HasSubstr("test-instance"));
        EXPECT_EQ("test-etag-1", p.policy.etag());
        return Status(StatusCode::kAborted, "test-msg");
      });

  InstanceAdminClient client(mock);
  auto actual = client.SetIamPolicy(
      Instance("test-project", "test-instance"),
      [](google::iam::v1::Policy p) { return p; }, RerunPolicyForTesting(),
      BackoffPolicyForTesting());
  EXPECT_THAT(actual, StatusIs(StatusCode::kAborted, HasSubstr("test-msg")));
}

TEST(InstanceAdminClientTest, TestIamPermissions) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, TestIamPermissions(_))
      .WillOnce([](InstanceAdminConnection::TestIamPermissionsParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        EXPECT_THAT(p.permissions,
                    ElementsAre("test.permission1", "test.permission2"));
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual =
      client.TestIamPermissions(Instance("test-project", "test-instance"),
                                {"test.permission1", "test.permission2"});
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
