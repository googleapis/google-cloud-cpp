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
#include "google/cloud/spanner/create_instance_request_builder.h"
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
using ::testing::AtLeast;
using ::testing::Return;

namespace gcsa = ::google::spanner::admin::instance::v1;
namespace giam = ::google::iam::v1;

// Create a `Connection` suitable for use in tests that continue retrying
// until the retry policy is exhausted - attempting that with the default
// policies would take too long (30 minutes).
// Other tests can use this method or just call `MakeInstanceAdminConnection()`
// directly.
std::shared_ptr<InstanceAdminConnection> MakeLimitedRetryConnection(
    std::shared_ptr<spanner_testing::MockInstanceAdminStub> mock) {
  LimitedErrorCountRetryPolicy retry(/*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  GenericPollingPolicy<LimitedErrorCountRetryPolicy> polling(retry, backoff);
  return internal::MakeInstanceAdminConnection(
      std::move(mock), retry.clone(), backoff.clone(), polling.clone());
}

TEST(InstanceAdminConnectionTest, GetInstanceSuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  auto constexpr kText = R"pb(
    name: "projects/test-project/instances/test-instance"
    config: "test-config"
    display_name: "test display name"
    node_count: 7
    state: CREATING
  )pb";
  gcsa::Instance expected_instance;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected_instance));

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstance(_, _))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::GetInstanceRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        return Status(StatusCode::kUnavailable, "try-again");
      })
      .WillOnce(
          [&expected_name, &expected_instance](
              grpc::ClientContext&, gcsa::GetInstanceRequest const& request) {
            EXPECT_EQ(expected_name, request.name());
            return expected_instance;
          });

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetInstance({expected_name});
  EXPECT_THAT(*actual, IsProtoEqual(expected_instance));
}

TEST(InstanceAdminConnectionTest, GetInstancePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstance(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetInstance({"test-name"});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminConnectionTest, GetInstanceTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstance(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetInstance({"test-name"});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(InstanceAdminClientTest, CreateInstanceSuccess) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  EXPECT_CALL(*mock, CreateInstance(_, _))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::CreateInstanceRequest const& r) {
        EXPECT_EQ("test-instance", r.instance_id());
        EXPECT_EQ("projects/test-project", r.parent());
        auto const& instance = r.instance();
        EXPECT_EQ(expected_name, instance.name());
        EXPECT_EQ("test-instance-config", instance.config());
        EXPECT_EQ("test-display-name", instance.display_name());
        auto const& labels = instance.labels();
        EXPECT_EQ(1, labels.count("key"));
        EXPECT_EQ("value", labels.at("key"));
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_status_or(op);
      });
  EXPECT_CALL(*mock, GetOperation(_, _))
      .WillOnce(
          [&expected_name](grpc::ClientContext&,
                           google::longrunning::GetOperationRequest const& r) {
            EXPECT_EQ("test-operation-name", r.name());
            google::longrunning::Operation op;
            op.set_name(r.name());
            op.set_done(true);
            gcsa::Instance instance;
            instance.set_name(expected_name);
            op.mutable_response()->PackFrom(instance);
            return make_status_or(op);
          });

  auto conn = MakeLimitedRetryConnection(std::move(mock));
  Instance in("test-project", "test-instance");
  auto fut = conn->CreateInstance(
      {CreateInstanceRequestBuilder(in, "test-instance-config")
           .SetDisplayName("test-display-name")
           .SetNodeCount(1)
           .SetLabels({{"key", "value"}})
           .Build()});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto instance = fut.get();
  EXPECT_STATUS_OK(instance);

  EXPECT_EQ(expected_name, instance->name());
}

TEST(InstanceAdminClientTest, CreateInstanceError) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();

  EXPECT_CALL(*mock, CreateInstance(_, _))
      .WillOnce([](grpc::ClientContext&, gcsa::CreateInstanceRequest const&) {
        return StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });

  auto conn = MakeLimitedRetryConnection(std::move(mock));
  Instance in("test-project", "test-instance");
  auto fut = conn->CreateInstance(
      {CreateInstanceRequestBuilder(in, "test-instance-config")
           .SetDisplayName("test-display-name")
           .SetNodeCount(1)
           .SetLabels({{"key", "value"}})
           .Build()});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto instance = fut.get();
  EXPECT_EQ(StatusCode::kPermissionDenied, instance.status().code());
}

TEST(InstanceAdminClientTest, UpdateInstanceSuccess) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  std::string expected_name = "projects/test-project/instances/test-instance";

  EXPECT_CALL(*mock, UpdateInstance(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::UpdateInstanceRequest const& r) {
        EXPECT_EQ(expected_name, r.instance().name());
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_status_or(op);
      });
  EXPECT_CALL(*mock, GetOperation(_, _))
      .WillOnce(
          [&expected_name](grpc::ClientContext&,
                           google::longrunning::GetOperationRequest const& r) {
            EXPECT_EQ("test-operation-name", r.name());
            google::longrunning::Operation op;
            op.set_name(r.name());
            op.set_done(true);
            gcsa::Instance instance;
            instance.set_name(expected_name);
            op.mutable_response()->PackFrom(instance);
            return make_status_or(op);
          });

  auto conn = MakeLimitedRetryConnection(std::move(mock));
  gcsa::UpdateInstanceRequest req;
  req.mutable_instance()->set_name(expected_name);
  auto fut = conn->UpdateInstance({req});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto instance = fut.get();
  EXPECT_STATUS_OK(instance);

  EXPECT_EQ(expected_name, instance->name());
}

TEST(InstanceAdminClientTest, UpdateInstancePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();

  EXPECT_CALL(*mock, UpdateInstance(_, _))
      .WillOnce([](grpc::ClientContext&, gcsa::UpdateInstanceRequest const&) {
        return StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });

  auto conn = MakeLimitedRetryConnection(std::move(mock));
  auto fut = conn->UpdateInstance({gcsa::UpdateInstanceRequest()});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto instance = fut.get();
  EXPECT_EQ(StatusCode::kPermissionDenied, instance.status().code());
}

TEST(InstanceAdminClientTest, UpdateInstanceTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();

  EXPECT_CALL(*mock, UpdateInstance(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(
          [](grpc::ClientContext&, gcsa::UpdateInstanceRequest const&) {
            return StatusOr<google::longrunning::Operation>(
                Status(StatusCode::kUnavailable, "try-again"));
          });
  auto conn = MakeLimitedRetryConnection(std::move(mock));
  auto fut = conn->UpdateInstance({gcsa::UpdateInstanceRequest()});
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto instance = fut.get();
  EXPECT_EQ(StatusCode::kUnavailable, instance.status().code());
}

TEST(InstanceAdminConnectionTest, DeleteInstanceSuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, DeleteInstance(_, _))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::DeleteInstanceRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        return Status(StatusCode::kUnavailable, "try-again");
      })
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::DeleteInstanceRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        return Status();
      });

  auto conn = MakeLimitedRetryConnection(mock);
  auto status = conn->DeleteInstance({expected_name});
  ASSERT_STATUS_OK(status);
}

TEST(InstanceAdminConnectionTest, DeleteInstancePermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, DeleteInstance(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto status = conn->DeleteInstance({"test-name"});
  EXPECT_EQ(StatusCode::kPermissionDenied, status.code());
}

TEST(InstanceAdminConnectionTest, DeleteInstanceTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, DeleteInstance(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto status = conn->DeleteInstance({"test-name"});
  EXPECT_EQ(StatusCode::kUnavailable, status.code());
}

TEST(InstanceAdminConnectionTest, GetInstanceConfigSuccess) {
  std::string const expected_name =
      "projects/test-project/instanceConfigs/test-instance-config";
  auto constexpr kText = R"pb(
    name: "projects/test-project/instanceConfigs/test-instance-config"
    display_name: "test display name"
  )pb";
  gcsa::InstanceConfig expected_instance_config;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected_instance_config));

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstanceConfig(_, _))
      .WillOnce(
          [&expected_name](grpc::ClientContext&,
                           gcsa::GetInstanceConfigRequest const& request) {
            EXPECT_EQ(expected_name, request.name());
            return Status(StatusCode::kUnavailable, "try-again");
          })
      .WillOnce([&expected_name, &expected_instance_config](
                    grpc::ClientContext&,
                    gcsa::GetInstanceConfigRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        return expected_instance_config;
      });

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetInstanceConfig({expected_name});
  EXPECT_THAT(*actual, IsProtoEqual(expected_instance_config));
}

TEST(InstanceAdminConnectionTest, GetInstanceConfigPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstanceConfig(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual =
      conn->GetInstanceConfig({"projects/test/instanceConfig/test-name"});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminConnectionTest, GetInstanceConfigTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetInstanceConfig(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual =
      conn->GetInstanceConfig({"projects/test/instanceConfig/test-name"});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(InstanceAdminConnectionTest, ListInstanceConfigsSuccess) {
  std::string const expected_parent = "projects/test-project";

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstanceConfigs(_, _))
      .WillOnce([&](grpc::ClientContext&,
                    gcsa::ListInstanceConfigsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());
        return Status(StatusCode::kUnavailable, "try-again");
      })
      .WillOnce([&](grpc::ClientContext&,
                    gcsa::ListInstanceConfigsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());

        gcsa::ListInstanceConfigsResponse response;
        response.set_next_page_token("p1");
        response.add_instance_configs()->set_name("c1");
        response.add_instance_configs()->set_name("c2");
        return response;
      })
      .WillOnce([&](grpc::ClientContext&,
                    gcsa::ListInstanceConfigsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("p1", request.page_token());

        gcsa::ListInstanceConfigsResponse response;
        response.clear_next_page_token();
        response.add_instance_configs()->set_name("c3");
        return response;
      });

  auto conn = MakeLimitedRetryConnection(mock);
  std::vector<std::string> actual_names;
  for (auto const& instance_config :
       conn->ListInstanceConfigs({"test-project"})) {
    ASSERT_STATUS_OK(instance_config);
    actual_names.push_back(instance_config->name());
  }
  EXPECT_THAT(actual_names, ::testing::ElementsAre("c1", "c2", "c3"));
}

TEST(InstanceAdminConnectionTest, ListInstanceConfigsPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstanceConfigs(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto range = conn->ListInstanceConfigs({"test-project"});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(InstanceAdminConnectionTest, ListInstanceConfigsTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstanceConfigs(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto range = conn->ListInstanceConfigs({"test-project"});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kUnavailable, begin->status().code());
}

TEST(InstanceAdminConnectionTest, ListInstancesSuccess) {
  std::string const expected_parent = "projects/test-project";

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstances(_, _))
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::ListInstancesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("labels.test-key:test-value", request.filter());
            EXPECT_TRUE(request.page_token().empty());
            return Status(StatusCode::kUnavailable, "try-again");
          })
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::ListInstancesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("labels.test-key:test-value", request.filter());
            EXPECT_TRUE(request.page_token().empty());

            gcsa::ListInstancesResponse response;
            response.set_next_page_token("p1");
            response.add_instances()->set_name("i1");
            response.add_instances()->set_name("i2");
            return response;
          })
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::ListInstancesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("labels.test-key:test-value", request.filter());
            EXPECT_EQ("p1", request.page_token());

            gcsa::ListInstancesResponse response;
            response.clear_next_page_token();
            response.add_instances()->set_name("i3");
            return response;
          });

  auto conn = MakeLimitedRetryConnection(mock);
  std::vector<std::string> actual_names;
  for (auto const& instance :
       conn->ListInstances({"test-project", "labels.test-key:test-value"})) {
    ASSERT_STATUS_OK(instance);
    actual_names.push_back(instance->name());
  }
  EXPECT_THAT(actual_names, ::testing::ElementsAre("i1", "i2", "i3"));
}

TEST(InstanceAdminConnectionTest, ListInstancesPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstances(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto range = conn->ListInstances({"test-project", ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(InstanceAdminConnectionTest, ListInstancesTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, ListInstances(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto range = conn->ListInstances({"test-project", ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kUnavailable, begin->status().code());
}

TEST(InstanceAdminConnectionTest, GetIamPolicySuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetIamPolicy(_, _))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 giam::GetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        return Status(StatusCode::kUnavailable, "try-again");
      })
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 giam::GetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        giam::Policy response;
        auto& binding = *response.add_bindings();
        binding.set_role("roles/spanner.databaseReader");
        binding.add_members("user:test-account@example.com");
        return response;
      });

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetIamPolicy({expected_name});
  ASSERT_STATUS_OK(actual);
  ASSERT_EQ(1, actual->bindings_size());
  EXPECT_EQ("roles/spanner.databaseReader", actual->bindings(0).role());
  ASSERT_EQ(1, actual->bindings(0).members_size());
  ASSERT_EQ("user:test-account@example.com", actual->bindings(0).members(0));
}

TEST(InstanceAdminConnectionTest, GetIamPolicyPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetIamPolicy(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetIamPolicy({"test-instance-name"});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminConnectionTest, GetIamPolicyTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, GetIamPolicy(_, _))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->GetIamPolicy({"test-instance-name"});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(InstanceAdminConnectionTest, SetIamPolicySuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  auto constexpr kText = R"pb(
    etag: "request-etag"
    bindings {
      role: "roles/spanner.databaseReader"
      members: "user:test-user-1@example.com"
      members: "user:test-user-2@example.com"
    }
  )pb";
  giam::Policy expected_policy;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected_policy));

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy(_, _))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 giam::SetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        return Status(StatusCode::kUnavailable, "try-again");
      })
      .WillOnce(
          [&expected_name, &expected_policy](
              grpc::ClientContext&, giam::SetIamPolicyRequest const& request) {
            EXPECT_EQ(expected_name, request.resource());
            EXPECT_THAT(request.policy(), IsProtoEqual(expected_policy));
            giam::Policy response = expected_policy;
            response.set_etag("response-etag");
            return response;
          });

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->SetIamPolicy({expected_name, expected_policy});
  ASSERT_STATUS_OK(actual);
  expected_policy.set_etag("response-etag");
  EXPECT_THAT(*actual, IsProtoEqual(expected_policy));
}

TEST(InstanceAdminConnectionTest, SetIamPolicyPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->SetIamPolicy({"test-instance-name", {}});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminConnectionTest, SetIamPolicyNonIdempotent) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  // If the Etag field is not set, then the RPC is not idempotent and should
  // fail on the first transient error.
  EXPECT_CALL(*mock, SetIamPolicy(_, _))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  google::iam::v1::Policy policy;
  auto actual = conn->SetIamPolicy({"test-instance-name", policy});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(InstanceAdminConnectionTest, SetIamPolicyIdempotent) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  google::iam::v1::Policy policy;
  policy.set_etag("test-etag-value");
  auto actual = conn->SetIamPolicy({"test-instance-name", policy});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(InstanceAdminConnectionTest, TestIamPermissionsSuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance";

  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, TestIamPermissions(_, _))
      .WillOnce(
          [&expected_name](grpc::ClientContext&,
                           giam::TestIamPermissionsRequest const& request) {
            EXPECT_EQ(expected_name, request.resource());
            return Status(StatusCode::kUnavailable, "try-again");
          })
      .WillOnce(
          [&expected_name](grpc::ClientContext&,
                           giam::TestIamPermissionsRequest const& request) {
            EXPECT_EQ(expected_name, request.resource());
            giam::TestIamPermissionsResponse response;
            response.add_permissions("test.permission2");
            return response;
          });

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual = conn->TestIamPermissions(
      {expected_name, {"test.permission1", "test.permission2"}});
  ASSERT_STATUS_OK(actual);
  ASSERT_EQ(1, actual->permissions_size());
  ASSERT_EQ("test.permission2", actual->permissions(0));
}

TEST(InstanceAdminConnectionTest, TestIamPermissionsPermanentFailure) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, TestIamPermissions(_, _))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual =
      conn->TestIamPermissions({"test-instance-name", {"test.permission"}});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminConnectionTest, TestIamPermissionsTooManyTransients) {
  auto mock = std::make_shared<spanner_testing::MockInstanceAdminStub>();
  EXPECT_CALL(*mock, TestIamPermissions(_, _))
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = MakeLimitedRetryConnection(mock);
  auto actual =
      conn->TestIamPermissions({"test-instance-name", {"test.permission"}});
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
