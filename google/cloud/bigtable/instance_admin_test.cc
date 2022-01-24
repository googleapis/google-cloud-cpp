// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/admin/mocks/mock_bigtable_instance_admin_connection.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Helper class for checking that the legacy API still functions correctly
class InstanceAdminTester {
 public:
  static std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection>
  Connection(InstanceAdmin const& admin) {
    return admin.connection_;
  }

  static Options Policies(InstanceAdmin const& admin) {
    return admin.policies_;
  }
};

namespace {

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockPollingPolicy;
using ::google::cloud::bigtable::testing::MockRetryPolicy;
using ::testing::An;

using MockConnection =
    ::google::cloud::bigtable_admin_mocks::MockBigtableInstanceAdminConnection;

std::string const kProjectId = "the-project";

void CheckPolicies(Options const& options) {
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminRetryPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminBackoffPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminPollingPolicyOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());
}

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  std::shared_ptr<MockConnection> connection_ =
      std::make_shared<MockConnection>();
};

TEST_F(InstanceAdminTest, Project) {
  Project p(kProjectId);
  InstanceAdmin tested(MakeInstanceAdminClient(p.project_id()));
  EXPECT_EQ(p.project_id(), tested.project_id());
  EXPECT_EQ(p.FullName(), tested.project_name());
}

TEST_F(InstanceAdminTest, CopyConstructor) {
  auto source = InstanceAdmin(connection_, kProjectId);
  std::string const& expected = source.project_id();
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  InstanceAdmin copy(source);
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, MoveConstructor) {
  auto source = InstanceAdmin(connection_, kProjectId);
  std::string expected = source.project_id();
  InstanceAdmin copy(std::move(source));
  EXPECT_EQ(expected, copy.project_id());
}

TEST_F(InstanceAdminTest, CopyAssignment) {
  std::shared_ptr<MockConnection> other_client =
      std::make_shared<MockConnection>();

  auto source = InstanceAdmin(connection_, kProjectId);
  std::string const& expected = source.project_id();
  auto dest = InstanceAdmin(other_client, "other-project");
  EXPECT_NE(expected, dest.project_id());
  dest = source;
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, MoveAssignment) {
  std::shared_ptr<MockConnection> other_client =
      std::make_shared<MockConnection>();

  auto source = InstanceAdmin(connection_, kProjectId);
  std::string expected = source.project_id();
  auto dest = InstanceAdmin(other_client, "other-project");
  EXPECT_NE(expected, dest.project_id());
  dest = std::move(source);
  EXPECT_EQ(expected, dest.project_id());
}

TEST_F(InstanceAdminTest, LegacyConstructorSharesConnection) {
  auto admin_client = MakeInstanceAdminClient("test-project");
  auto admin_1 = InstanceAdmin(admin_client);
  auto admin_2 = InstanceAdmin(admin_client);
  auto conn_1 = InstanceAdminTester::Connection(admin_1);
  auto conn_2 = InstanceAdminTester::Connection(admin_2);

  EXPECT_EQ(conn_1, conn_2);
}

TEST_F(InstanceAdminTest, LegacyConstructorDefaultsPolicies) {
  auto admin_client = MakeInstanceAdminClient("test-project");
  auto admin = InstanceAdmin(std::move(admin_client));
  auto policies = InstanceAdminTester::Policies(admin);
  CheckPolicies(policies);
}

TEST_F(InstanceAdminTest, LegacyConstructorWithPolicies) {
  // In this test, we make a series of simple calls to verify that the policies
  // passed to the `InstanceAdmin` constructor are actually collected as
  // `Options`.
  //
  // Upon construction of an InstanceAdmin, each policy is cloned twice: Once
  // while processing the variadic parameters, once while converting from
  // Bigtable policies to common policies. This should explain the nested mocks
  // below.

  auto mock_r = std::make_shared<MockRetryPolicy>();
  auto mock_b = std::make_shared<MockBackoffPolicy>();
  auto mock_p = std::make_shared<MockPollingPolicy>();

  EXPECT_CALL(*mock_r, clone).WillOnce([] {
    auto clone_1 = absl::make_unique<MockRetryPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([] {
      auto clone_2 = absl::make_unique<MockRetryPolicy>();
      EXPECT_CALL(*clone_2, OnFailure(An<Status const&>()));
      return clone_2;
    });
    return clone_1;
  });

  EXPECT_CALL(*mock_b, clone).WillOnce([] {
    auto clone_1 = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([] {
      auto clone_2 = absl::make_unique<MockBackoffPolicy>();
      EXPECT_CALL(*clone_2, OnCompletion(An<Status const&>()));
      return clone_2;
    });
    return clone_1;
  });

  EXPECT_CALL(*mock_p, clone).WillOnce([] {
    auto clone_1 = absl::make_unique<MockPollingPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([] {
      auto clone_2 = absl::make_unique<MockPollingPolicy>();
      EXPECT_CALL(*clone_2, WaitPeriod);
      return clone_2;
    });
    return clone_1;
  });

  auto admin_client = MakeInstanceAdminClient("test-project");
  auto admin =
      InstanceAdmin(std::move(admin_client), *mock_r, *mock_b, *mock_p);
  auto policies = InstanceAdminTester::Policies(admin);
  CheckPolicies(policies);

  auto const& common_retry =
      policies.get<bigtable_admin::BigtableInstanceAdminRetryPolicyOption>();
  (void)common_retry->OnFailure({});

  auto const& common_backoff =
      policies.get<bigtable_admin::BigtableInstanceAdminBackoffPolicyOption>();
  (void)common_backoff->OnCompletion();

  auto const& common_polling =
      policies.get<bigtable_admin::BigtableInstanceAdminPollingPolicyOption>();
  (void)common_polling->WaitPeriod();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
