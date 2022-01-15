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
#include "google/cloud/grpc_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

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
 * This test uses custom policies to ensure that:
 *
 * 1. The retry and backokff `Setup()` function is called by invoking the
 *    `GrpcSetupOption` and not the `GrpcSetupPollOption`.
 *
 * 2. The polling `Setup()` function is called by invoking the
 *    `GrpcSetupPollOption` and not the `GrpcSetupOption`.
 *
 * 3. `GrpcSetupOption` and `GrpcSetupPollOption` use the initial state of the
 *    policies to setup the `grpc::ClientContext`.
 */
TEST(ConvertPolicies, GrpcSetup) {
  class RetryWithSetup : public RPCRetryPolicy {
   public:
    explicit RetryWithSetup(bool* called) : called_(called) {
      *called_ = false;
    }
    std::unique_ptr<RPCRetryPolicy> clone() const override {
      return absl::make_unique<RetryWithSetup>(called_);
    }
    void Setup(grpc::ClientContext&) const override {
      EXPECT_FALSE(*called_)
          << "GrpcSetupOption is not using the retry policy's initial state to "
             "configure the context";
      *called_ = true;
    }
    bool OnFailure(Status const&) override { return false; }
    bool OnFailure(grpc::Status const&) override { return false; }

   private:
    bool* called_;
  };

  class BackoffWithSetup : public RPCBackoffPolicy {
   public:
    explicit BackoffWithSetup(bool* called) : called_(called) {
      *called_ = false;
    }
    std::unique_ptr<RPCBackoffPolicy> clone() const override {
      return absl::make_unique<BackoffWithSetup>(called_);
    }
    void Setup(grpc::ClientContext&) const override {
      EXPECT_FALSE(*called_)
          << "GrpcSetupOption is not using the backoff policy's initial state "
             "to configure the context";
      *called_ = true;
    }
    std::chrono::milliseconds OnCompletion(Status const&) override {
      return {};
    };
    std::chrono::milliseconds OnCompletion(grpc::Status const&) override {
      return {};
    }

   private:
    bool* called_;
  };

  class PollingWithSetup : public PollingPolicy {
   public:
    explicit PollingWithSetup(bool* called) : called_(called) {
      *called_ = false;
    }
    std::unique_ptr<PollingPolicy> clone() const override {
      return absl::make_unique<PollingWithSetup>(called_);
    }
    void Setup(grpc::ClientContext&) override {
      EXPECT_FALSE(*called_)
          << "GrpcSetupPollOption is not using the polling policy's initial "
             "state to configure the context";
      *called_ = true;
    }
    bool IsPermanentError(grpc::Status const&) override { return false; }
    bool IsPermanentError(Status const&) override { return false; }
    bool OnFailure(grpc::Status const&) override { return false; }
    bool OnFailure(Status const&) override { return false; }
    bool Exhausted() override { return false; }
    std::chrono::milliseconds WaitPeriod() override { return {}; }

   private:
    bool* called_;
  };

  grpc::ClientContext context;
  bool retry_called = false;
  bool backoff_called = false;
  bool polling_called = false;

  auto r = std::make_shared<RetryWithSetup>(&retry_called);
  auto b = std::make_shared<BackoffWithSetup>(&backoff_called);
  auto p = std::make_shared<PollingWithSetup>(&polling_called);
  auto options = bigtable_internal::MakeGrpcSetupOptions(
      std::move(r), std::move(b), std::move(p));
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupOption>());
  EXPECT_TRUE(options.has<google::cloud::internal::GrpcSetupPollOption>());

  // Verify that Setup has not been called for any of the policies.
  EXPECT_FALSE(retry_called);
  EXPECT_FALSE(backoff_called);
  EXPECT_FALSE(polling_called);

  // Use `GrpcSetupOption`
  google::cloud::internal::ConfigureContext(context, options);

  EXPECT_TRUE(retry_called);
  EXPECT_TRUE(backoff_called);
  EXPECT_FALSE(polling_called);

  // Verify that we only ever use the policies' initial setups
  google::cloud::internal::ConfigureContext(context, options);

  // Reset the flags
  retry_called = false;
  backoff_called = false;

  // Use `GrpcSetupPollOption`
  google::cloud::internal::ConfigurePollContext(context, options);

  EXPECT_FALSE(retry_called);
  EXPECT_FALSE(backoff_called);
  EXPECT_TRUE(polling_called);

  // Verify that we only ever use the policy's initial setup
  google::cloud::internal::ConfigurePollContext(context, options);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
