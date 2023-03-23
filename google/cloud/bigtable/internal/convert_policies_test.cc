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

#include "google/cloud/bigtable/internal/convert_policies.h"
#include "google/cloud/bigtable/admin/bigtable_instance_admin_options.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_options.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/grpc_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockPollingPolicy;
using ::google::cloud::bigtable::testing::MockRetryPolicy;

TEST(ConvertPolicies, InstanceAdmin) {
  auto r = DefaultRPCRetryPolicy(internal::kBigtableInstanceAdminLimits);
  auto b = DefaultRPCBackoffPolicy(internal::kBigtableInstanceAdminLimits);
  auto p = DefaultPollingPolicy(internal::kBigtableInstanceAdminLimits);
  auto options = bigtable_internal::MakeInstanceAdminOptions(
      std::move(r), std::move(b), std::move(p));
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminRetryPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminBackoffPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableInstanceAdminPollingPolicyOption>());
}

TEST(ConvertPolicies, TableAdmin) {
  auto r = DefaultRPCRetryPolicy(internal::kBigtableTableAdminLimits);
  auto b = DefaultRPCBackoffPolicy(internal::kBigtableTableAdminLimits);
  auto p = DefaultPollingPolicy(internal::kBigtableTableAdminLimits);
  auto options = bigtable_internal::MakeTableAdminOptions(
      std::move(r), std::move(b), std::move(p));
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminRetryPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminBackoffPolicyOption>());
  EXPECT_TRUE(
      options.has<bigtable_admin::BigtableTableAdminPollingPolicyOption>());
}

/**
 * This test converts policies into options, then invokes the `GrpcSetupOption`
 * twice. It checks that:
 *
 *   - `clone()` is called twice for both the retry and backoff policies.
 *   - `Setup()` is never called more than once by a clone.
 *   - The polling policy is untouched.
 */
TEST(ConvertPolicies, GrpcSetupOption) {
  auto mock_r = std::make_shared<MockRetryPolicy>();
  auto mock_b = std::make_shared<MockBackoffPolicy>();
  auto mock_p = std::make_shared<MockPollingPolicy>();

  EXPECT_CALL(*mock_r, Setup).Times(0);
  EXPECT_CALL(*mock_b, Setup).Times(0);
  EXPECT_CALL(*mock_p, Setup).Times(0);

  EXPECT_CALL(*mock_r, clone)
      .WillOnce([]() {
        auto clone = std::make_unique<MockRetryPolicy>();
        EXPECT_CALL(*clone, Setup).Times(1);
        return clone;
      })
      .WillOnce([]() {
        auto clone = std::make_unique<MockRetryPolicy>();
        EXPECT_CALL(*clone, Setup).Times(1);
        return clone;
      });
  EXPECT_CALL(*mock_b, clone)
      .WillOnce([]() {
        auto clone = std::make_unique<MockBackoffPolicy>();
        EXPECT_CALL(*clone, Setup).Times(1);
        return clone;
      })
      .WillOnce([]() {
        auto clone = std::make_unique<MockBackoffPolicy>();
        EXPECT_CALL(*clone, Setup).Times(1);
        return clone;
      });
  EXPECT_CALL(*mock_p, clone).Times(0);

  auto options =
      bigtable_internal::MakeGrpcSetupOptions(mock_r, mock_b, mock_p);

  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());

  grpc::ClientContext context;
  google::cloud::internal::ConfigureContext(context, options);
  google::cloud::internal::ConfigureContext(context, options);
}

/**
 * This test converts policies into options, then invokes the
 * `GrpcSetupPollOption` twice. It checks that:
 *
 *   - `clone()` is called twice for the polling policy.
 *   - `Setup()` is never called more than once by a clone.
 *   - Both the retry and backoff policies are untouched.
 */
TEST(ConvertPolicies, GrpcSetupPollOption) {
  auto mock_r = std::make_shared<MockRetryPolicy>();
  auto mock_b = std::make_shared<MockBackoffPolicy>();
  auto mock_p = std::make_shared<MockPollingPolicy>();

  EXPECT_CALL(*mock_r, Setup).Times(0);
  EXPECT_CALL(*mock_b, Setup).Times(0);
  EXPECT_CALL(*mock_p, Setup).Times(0);

  EXPECT_CALL(*mock_r, clone).Times(0);
  EXPECT_CALL(*mock_b, clone).Times(0);
  EXPECT_CALL(*mock_p, clone)
      .WillOnce([]() {
        auto clone = std::make_unique<MockPollingPolicy>();
        EXPECT_CALL(*clone, Setup).Times(1);
        return clone;
      })
      .WillOnce([]() {
        auto clone = std::make_unique<MockPollingPolicy>();
        EXPECT_CALL(*clone, Setup).Times(1);
        return clone;
      });

  auto options =
      bigtable_internal::MakeGrpcSetupOptions(mock_r, mock_b, mock_p);

  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());

  grpc::ClientContext context;
  google::cloud::internal::ConfigurePollContext(context, options);
  google::cloud::internal::ConfigurePollContext(context, options);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
