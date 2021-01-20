// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/iam_credentials_connection_idempotency_policy.gcpcxx.pb.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

using google::cloud::internal::Idempotency;

class IAMCredentialsIdempotencyPolicyTest : public ::testing::Test {
protected:
  void SetUp() override {
    policy_ = golden::MakeDefaultIAMCredentialsConnectionIdempotencyPolicy();
  }

  std::unique_ptr<golden::IAMCredentialsConnectionIdempotencyPolicy> policy_;
};

TEST_F(IAMCredentialsIdempotencyPolicyTest, GenerateAccessToken) {
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  EXPECT_EQ(policy_->GenerateAccessToken(request), Idempotency::kNonIdempotent);
}

TEST_F(IAMCredentialsIdempotencyPolicyTest, GenerateIdToken) {
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  EXPECT_EQ(policy_->GenerateIdToken(request), Idempotency::kNonIdempotent);
}

TEST_F(IAMCredentialsIdempotencyPolicyTest, WriteLogEntries) {
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  EXPECT_EQ(policy_->WriteLogEntries(request), Idempotency::kNonIdempotent);
}

TEST_F(IAMCredentialsIdempotencyPolicyTest, ListLogs) {
  google::test::admin::database::v1::ListLogsRequest request;
  EXPECT_EQ(policy_->ListLogs(request), Idempotency::kIdempotent);
}

} // namespace
} // namespace golden_internal
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
