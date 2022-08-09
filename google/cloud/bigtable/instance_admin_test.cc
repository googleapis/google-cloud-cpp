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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Helper class for checking that the legacy API still functions correctly
class InstanceAdminTester {
 public:
  static std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection>
  Connection(bigtable::InstanceAdmin const& admin) {
    return admin.connection_;
  }

  static ::google::cloud::Options Options(
      bigtable::InstanceAdmin const& admin) {
    return admin.options_;
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace btadmin = ::google::bigtable::admin::v2;
namespace iamproto = ::google::iam::v1;

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockPollingPolicy;
using ::google::cloud::bigtable::testing::MockRetryPolicy;
using ::google::cloud::bigtable_internal::InstanceAdminTester;
using ::google::cloud::testing_util::StatusIs;
using ::testing::An;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

using MockConnection =
    ::google::cloud::bigtable_admin_mocks::MockBigtableInstanceAdminConnection;

auto const kProjectId = "the-project";
auto const kInstanceId = "the-instance";
auto const kClusterId = "the-cluster";
auto const kProfileId = "the-profile";
auto const kProjectName = "projects/the-project";
auto const kInstanceName = "projects/the-project/instances/the-instance";
auto const kClusterName =
    "projects/the-project/instances/the-instance/clusters/the-cluster";
auto const kProfileName =
    "projects/the-project/instances/the-instance/appProfiles/the-profile";

std::string LocationName(std::string const& location) {
  return kProjectName + ("/locations/" + location);
}

Status FailingStatus() { return Status(StatusCode::kPermissionDenied, "fail"); }

struct TestOption {
  using Type = int;
};

Options TestOptions() {
  return Options{}
      .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
      .set<TestOption>(1);
}

void CheckOptions(Options const& options) {
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminRetryPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminBackoffPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminPollingPolicyOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());
  EXPECT_TRUE(options.has<TestOption>());
}

/// A fixture for the bigtable::InstanceAdmin tests.
class InstanceAdminTest : public ::testing::Test {
 protected:
  InstanceAdmin DefaultInstanceAdmin() {
    EXPECT_CALL(*connection_, options())
        .WillRepeatedly(Return(Options{}.set<TestOption>(1)));
    return InstanceAdmin(connection_, kProjectId);
  }

  std::shared_ptr<MockConnection> connection_ =
      std::make_shared<MockConnection>();
};

TEST_F(InstanceAdminTest, Project) {
  InstanceAdmin tested(MakeInstanceAdminClient(kProjectId, TestOptions()));
  EXPECT_EQ(kProjectId, tested.project_id());
  EXPECT_EQ(kProjectName, tested.project_name());
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

TEST_F(InstanceAdminTest, WithNewTarget) {
  auto admin = InstanceAdmin(connection_, kProjectId);
  auto other_admin = admin.WithNewTarget("other-project");
  EXPECT_EQ(other_admin.project_id(), "other-project");
  EXPECT_EQ(other_admin.project_name(), Project("other-project").FullName());
}

TEST_F(InstanceAdminTest, LegacyConstructorSharesConnection) {
  auto admin_client = MakeInstanceAdminClient("test-project", TestOptions());
  auto admin_1 = InstanceAdmin(admin_client);
  auto admin_2 = InstanceAdmin(admin_client);
  auto conn_1 = InstanceAdminTester::Connection(admin_1);
  auto conn_2 = InstanceAdminTester::Connection(admin_2);

  EXPECT_EQ(conn_1, conn_2);
  EXPECT_THAT(conn_1, NotNull());
}

TEST_F(InstanceAdminTest, LegacyConstructorDefaultsPolicies) {
  auto admin_client = MakeInstanceAdminClient("test-project", TestOptions());
  auto admin = InstanceAdmin(std::move(admin_client));
  auto options = InstanceAdminTester::Options(admin);
  CheckOptions(options);
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

  auto admin_client = MakeInstanceAdminClient("test-project", TestOptions());
  auto admin =
      InstanceAdmin(std::move(admin_client), *mock_r, *mock_b, *mock_p);
  auto options = InstanceAdminTester::Options(admin);
  CheckOptions(options);

  auto const& common_retry =
      options.get<bigtable_admin::BigtableInstanceAdminRetryPolicyOption>();
  (void)common_retry->OnFailure({});

  auto const& common_backoff =
      options.get<bigtable_admin::BigtableInstanceAdminBackoffPolicyOption>();
  (void)common_backoff->OnCompletion();

  auto const& common_polling =
      options.get<bigtable_admin::BigtableInstanceAdminPollingPolicyOption>();
  (void)common_polling->WaitPeriod();
}

TEST_F(InstanceAdminTest, ListInstancesSuccess) {
  auto tested = DefaultInstanceAdmin();
  std::vector<std::string> const expected_names = {
      InstanceName(kProjectId, "i0"), InstanceName(kProjectId, "i1")};
  std::vector<std::string> const expected_fails = {"l0", "l1"};

  EXPECT_CALL(*connection_, ListInstances)
      .WillOnce([&expected_names, &expected_fails](
                    btadmin::ListInstancesRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kProjectName, request.parent());

        btadmin::ListInstancesResponse response;
        for (auto const& name : expected_names) {
          auto& instance = *response.add_instances();
          instance.set_name(name);
        }
        for (auto const& loc : expected_fails) {
          *response.add_failed_locations() = loc;
        }
        return make_status_or(response);
      });

  auto actual = tested.ListInstances();
  ASSERT_STATUS_OK(actual);
  std::vector<std::string> actual_names;
  std::transform(actual->instances.begin(), actual->instances.end(),
                 std::back_inserter(actual_names),
                 [](btadmin::Instance const& i) { return i.name(); });

  EXPECT_THAT(actual_names, ElementsAreArray(expected_names));
  EXPECT_THAT(actual->failed_locations, ElementsAreArray(expected_fails));
}

TEST_F(InstanceAdminTest, ListInstancesFailure) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, ListInstances).WillOnce(Return(FailingStatus()));

  EXPECT_THAT(tested.ListInstances(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, CreateInstance) {
  auto tested = DefaultInstanceAdmin();
  auto constexpr kDisplayName = "display name";
  std::vector<std::string> const expected_location_names = {LocationName("l0"),
                                                            LocationName("l1")};
  std::map<std::string, ClusterConfig> cluster_map = {
      {"c0", ClusterConfig("l0", 3, btadmin::HDD)},
      {"c1", ClusterConfig("l1", 3, btadmin::HDD)}};
  auto config = InstanceConfig(kInstanceId, kDisplayName, cluster_map);

  EXPECT_CALL(*connection_, CreateInstance)
      .WillOnce([&](btadmin::CreateInstanceRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceId, request.instance_id());
        EXPECT_EQ(kProjectName, request.parent());
        EXPECT_EQ(kDisplayName, request.instance().display_name());
        std::vector<std::string> actual_location_names;
        for (auto&& c : request.clusters()) {
          actual_location_names.emplace_back(c.second.location());
        }
        EXPECT_THAT(actual_location_names,
                    UnorderedElementsAreArray(expected_location_names));
        return make_ready_future<StatusOr<btadmin::Instance>>(FailingStatus());
      });

  EXPECT_THAT(tested.CreateInstance(config).get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, CreateCluster) {
  auto tested = DefaultInstanceAdmin();
  auto const location_name = LocationName("the-location");
  auto config = ClusterConfig("the-location", 3, btadmin::HDD);

  EXPECT_CALL(*connection_, CreateCluster)
      .WillOnce([&](btadmin::CreateClusterRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterId, request.cluster_id());
        EXPECT_EQ(kInstanceName, request.parent());
        EXPECT_EQ(location_name, request.cluster().location());
        EXPECT_EQ(3, request.cluster().serve_nodes());
        EXPECT_EQ(btadmin::HDD, request.cluster().default_storage_type());
        return make_ready_future<StatusOr<btadmin::Cluster>>(FailingStatus());
      });

  EXPECT_THAT(tested.CreateCluster(config, kInstanceId, kClusterId).get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, UpdateInstance) {
  auto tested = DefaultInstanceAdmin();
  auto constexpr kDisplayName = "updated display name";
  InstanceUpdateConfig config({});
  config.set_display_name(kDisplayName);

  EXPECT_CALL(*connection_, PartialUpdateInstance)
      .WillOnce([&](btadmin::PartialUpdateInstanceRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kDisplayName, request.instance().display_name());
        EXPECT_THAT(request.update_mask().paths(), Contains("display_name"));
        return make_ready_future<StatusOr<btadmin::Instance>>(FailingStatus());
      });

  EXPECT_THAT(tested.UpdateInstance(config).get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, GetInstance) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, GetInstance)
      .WillOnce([&](btadmin::GetInstanceRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.name());
        return FailingStatus();
      });

  EXPECT_THAT(tested.GetInstance(kInstanceId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, DeleteInstance) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, DeleteInstance)
      .WillOnce([&](btadmin::DeleteInstanceRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.name());
        return Status();
      });

  EXPECT_STATUS_OK(tested.DeleteInstance(kInstanceId));
}

TEST_F(InstanceAdminTest, GetCluster) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, GetCluster)
      .WillOnce([&](btadmin::GetClusterRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterName, request.name());
        return FailingStatus();
      });

  EXPECT_THAT(tested.GetCluster(kInstanceId, kClusterId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, ListClustersSuccess) {
  auto tested = DefaultInstanceAdmin();
  std::vector<std::string> const expected_names = {
      ClusterName(kProjectId, kInstanceId, "c0"),
      ClusterName(kProjectId, kInstanceId, "c1")};
  std::vector<std::string> const expected_fails = {"l0", "l1"};

  EXPECT_CALL(*connection_, ListClusters)
      .WillOnce([&](btadmin::ListClustersRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.parent());

        btadmin::ListClustersResponse response;
        for (auto const& name : expected_names) {
          auto& cluster = *response.add_clusters();
          cluster.set_name(name);
        }
        for (auto const& loc : expected_fails) {
          *response.add_failed_locations() = loc;
        }
        return make_status_or(response);
      });

  auto actual = tested.ListClusters(kInstanceId);
  ASSERT_STATUS_OK(actual);
  std::vector<std::string> actual_names;
  std::transform(actual->clusters.begin(), actual->clusters.end(),
                 std::back_inserter(actual_names),
                 [](btadmin::Cluster const& c) { return c.name(); });

  EXPECT_THAT(actual_names, ElementsAreArray(expected_names));
  EXPECT_THAT(actual->failed_locations, ElementsAreArray(expected_fails));
}

TEST_F(InstanceAdminTest, ListClustersFailure) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, ListClusters)
      .WillOnce([&](btadmin::ListClustersRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        // Verify that calling `ListClusters` with no arguments sets the
        // instance-id to "-"
        auto const instance_name = InstanceName(kProjectId, "-");
        EXPECT_EQ(instance_name, request.parent());
        return FailingStatus();
      });

  EXPECT_THAT(tested.ListClusters(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, UpdateCluster) {
  auto tested = DefaultInstanceAdmin();
  auto const location_name = LocationName("the-location");
  btadmin::Cluster c;
  c.set_name(kClusterName);
  c.set_location(location_name);
  c.set_serve_nodes(3);
  c.set_default_storage_type(btadmin::HDD);
  auto config = ClusterConfig(std::move(c));

  EXPECT_CALL(*connection_, UpdateCluster)
      .WillOnce([&](btadmin::Cluster const& cluster) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterName, cluster.name());
        EXPECT_EQ(location_name, cluster.location());
        EXPECT_EQ(3, cluster.serve_nodes());
        EXPECT_EQ(btadmin::HDD, cluster.default_storage_type());
        return make_ready_future<StatusOr<btadmin::Cluster>>(FailingStatus());
      });

  EXPECT_THAT(tested.UpdateCluster(config).get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, DeleteCluster) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, DeleteCluster)
      .WillOnce([&](btadmin::DeleteClusterRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kClusterName, request.name());
        return Status();
      });

  EXPECT_STATUS_OK(tested.DeleteCluster(kInstanceId, kClusterId));
}

TEST_F(InstanceAdminTest, CreateAppProfile) {
  auto tested = DefaultInstanceAdmin();
  auto config = AppProfileConfig::MultiClusterUseAny(kProfileId);

  EXPECT_CALL(*connection_, CreateAppProfile)
      .WillOnce([&](btadmin::CreateAppProfileRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kProfileId, request.app_profile_id());
        EXPECT_EQ(kInstanceName, request.parent());
        return FailingStatus();
      });

  EXPECT_THAT(tested.CreateAppProfile(kInstanceId, config),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, GetAppProfile) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, GetAppProfile)
      .WillOnce([&](btadmin::GetAppProfileRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kProfileName, request.name());
        return FailingStatus();
      });

  EXPECT_THAT(tested.GetAppProfile(kInstanceId, kProfileId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, UpdateAppProfile) {
  auto tested = DefaultInstanceAdmin();
  auto constexpr kDescription = "description";
  auto config = AppProfileUpdateConfig().set_description(kDescription);

  EXPECT_CALL(*connection_, UpdateAppProfile)
      .WillOnce([&](btadmin::UpdateAppProfileRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kProfileName, request.app_profile().name());
        EXPECT_EQ(kDescription, request.app_profile().description());
        return make_ready_future<StatusOr<btadmin::AppProfile>>(
            FailingStatus());
      });

  EXPECT_THAT(tested.UpdateAppProfile(kInstanceId, kProfileId, config).get(),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, ListAppProfilesSuccess) {
  auto tested = DefaultInstanceAdmin();
  std::vector<std::string> const expected_names = {
      AppProfileName(kProjectId, kInstanceId, "p0"),
      AppProfileName(kProjectId, kInstanceId, "p1")};

  auto iter = expected_names.begin();
  EXPECT_CALL(*connection_, ListAppProfiles)
      .WillOnce([&iter, &expected_names](
                    btadmin::ListAppProfilesRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.parent());

        using ::google::cloud::internal::MakeStreamRange;
        using ::google::cloud::internal::StreamReader;
        auto reader = [&iter, &expected_names]()
            -> StreamReader<btadmin::AppProfile>::result_type {
          if (iter != expected_names.end()) {
            btadmin::AppProfile p;
            p.set_name(*iter);
            ++iter;
            return p;
          }
          return Status();
        };
        return MakeStreamRange<btadmin::AppProfile>(std::move(reader));
      });

  auto profiles = tested.ListAppProfiles(kInstanceId);
  ASSERT_STATUS_OK(profiles);
  std::vector<std::string> names;
  std::transform(profiles->begin(), profiles->end(), std::back_inserter(names),
                 [](btadmin::AppProfile const& p) { return p.name(); });

  EXPECT_THAT(names, ElementsAreArray(expected_names));
}

TEST_F(InstanceAdminTest, ListAppProfilesFailure) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, ListAppProfiles)
      .WillOnce([&](btadmin::ListAppProfilesRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.parent());

        using ::google::cloud::internal::MakeStreamRange;
        return MakeStreamRange<btadmin::AppProfile>(
            [] { return FailingStatus(); });
      });

  EXPECT_THAT(tested.ListAppProfiles(kInstanceId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, DeleteAppProfile) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, DeleteAppProfile)
      .WillOnce([&](btadmin::DeleteAppProfileRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kProfileName, request.name());
        EXPECT_EQ(true, request.ignore_warnings());
        return Status();
      });

  EXPECT_STATUS_OK(tested.DeleteAppProfile(kInstanceId, kProfileId, true));
}

TEST_F(InstanceAdminTest, GetNativeIamPolicy) {
  auto tested = DefaultInstanceAdmin();

  EXPECT_CALL(*connection_, GetIamPolicy)
      .WillOnce([&](iamproto::GetIamPolicyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.resource());
        return FailingStatus();
      });

  EXPECT_THAT(tested.GetNativeIamPolicy(kInstanceId),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, SetNativeIamPolicy) {
  auto tested = DefaultInstanceAdmin();
  iamproto::Policy policy;
  policy.set_etag("tag");
  policy.set_version(3);

  EXPECT_CALL(*connection_, SetIamPolicy)
      .WillOnce([&](iamproto::SetIamPolicyRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.resource());
        EXPECT_EQ("tag", request.policy().etag());
        EXPECT_EQ(3, request.policy().version());
        return FailingStatus();
      });

  EXPECT_THAT(tested.SetIamPolicy(kInstanceId, policy),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(InstanceAdminTest, TestIamPermissionsSuccess) {
  auto tested = DefaultInstanceAdmin();
  std::vector<std::string> const expected_permissions = {"writer", "reader"};
  std::vector<std::string> const returned_permissions = {"reader"};

  EXPECT_CALL(*connection_, TestIamPermissions)
      .WillOnce([&](iamproto::TestIamPermissionsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.resource());
        std::vector<std::string> actual_permissions;
        for (auto const& c : request.permissions()) {
          actual_permissions.emplace_back(c);
        }
        EXPECT_THAT(actual_permissions,
                    UnorderedElementsAreArray(expected_permissions));

        iamproto::TestIamPermissionsResponse r;
        for (auto const& p : returned_permissions) r.add_permissions(p);
        return r;
      });

  auto resp = tested.TestIamPermissions(kInstanceId, expected_permissions);
  ASSERT_STATUS_OK(resp);
  EXPECT_THAT(*resp, ElementsAreArray(returned_permissions));
}

TEST_F(InstanceAdminTest, TestIamPermissionsFailure) {
  auto tested = DefaultInstanceAdmin();
  std::vector<std::string> const expected_permissions = {"writer", "reader"};

  EXPECT_CALL(*connection_, TestIamPermissions)
      .WillOnce([&](iamproto::TestIamPermissionsRequest const& request) {
        CheckOptions(google::cloud::internal::CurrentOptions());
        EXPECT_EQ(kInstanceName, request.resource());
        std::vector<std::string> actual_permissions;
        for (auto const& c : request.permissions()) {
          actual_permissions.emplace_back(c);
        }
        EXPECT_THAT(actual_permissions,
                    UnorderedElementsAreArray(expected_permissions));
        return FailingStatus();
      });

  EXPECT_THAT(tested.TestIamPermissions(kInstanceId, expected_permissions),
              StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
