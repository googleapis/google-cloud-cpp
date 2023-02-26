// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/v1/golden_thing_admin_connection_idempotency_policy.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::Idempotency;
namespace gtab = ::google::test::admin::database::v1;

class GoldenIdempotencyPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    policy_ =
        golden_v1::MakeDefaultGoldenThingAdminConnectionIdempotencyPolicy();
  }

  std::unique_ptr<golden_v1::GoldenThingAdminConnectionIdempotencyPolicy>
      policy_;
};

TEST_F(GoldenIdempotencyPolicyTest, ListDatabases) {
  gtab::ListDatabasesRequest request;
  EXPECT_EQ(policy_->ListDatabases(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, CreateDatabase) {
  gtab::CreateDatabaseRequest request;
  EXPECT_EQ(policy_->CreateDatabase(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, GetDatabase) {
  gtab::GetDatabaseRequest request;
  EXPECT_EQ(policy_->GetDatabase(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, UpdateDatabaseDdl) {
  gtab::UpdateDatabaseDdlRequest request;
  EXPECT_EQ(policy_->UpdateDatabaseDdl(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, DropDatabase) {
  gtab::DropDatabaseRequest request;
  EXPECT_EQ(policy_->DropDatabase(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, GetDatabaseDdl) {
  gtab::GetDatabaseDdlRequest request;
  EXPECT_EQ(policy_->GetDatabaseDdl(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, SetIamPolicy) {
  google::iam::v1::SetIamPolicyRequest request;
  EXPECT_EQ(policy_->SetIamPolicy(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, GetIamPolicy) {
  google::iam::v1::GetIamPolicyRequest request;
  EXPECT_EQ(policy_->GetIamPolicy(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, TestIamPermissions) {
  google::iam::v1::TestIamPermissionsRequest request;
  EXPECT_EQ(policy_->TestIamPermissions(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, CreateBackup) {
  gtab::CreateBackupRequest request;
  EXPECT_EQ(policy_->CreateBackup(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, GetBackup) {
  gtab::GetBackupRequest request;
  EXPECT_EQ(policy_->GetBackup(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, UpdateBackup) {
  gtab::UpdateBackupRequest request;
  EXPECT_EQ(policy_->UpdateBackup(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, DeleteBackup) {
  gtab::DeleteBackupRequest request;
  EXPECT_EQ(policy_->DeleteBackup(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, ListBackups) {
  gtab::ListBackupsRequest request;
  EXPECT_EQ(policy_->ListBackups(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, RestoreDatabase) {
  gtab::RestoreDatabaseRequest request;
  EXPECT_EQ(policy_->RestoreDatabase(request), Idempotency::kNonIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, ListDatabaseOperations) {
  gtab::ListDatabaseOperationsRequest request;
  EXPECT_EQ(policy_->ListDatabaseOperations(request), Idempotency::kIdempotent);
}

TEST_F(GoldenIdempotencyPolicyTest, ListBackupOperations) {
  gtab::ListBackupOperationsRequest request;
  EXPECT_EQ(policy_->ListBackupOperations(request), Idempotency::kIdempotent);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
