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

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options MakeGrpcSetupOptions(
    std::shared_ptr<bigtable::RPCRetryPolicy> const& retry,
    std::shared_ptr<bigtable::RPCBackoffPolicy> const& backoff,
    std::shared_ptr<bigtable::PollingPolicy> const& polling) {
  auto setup = [retry, backoff](grpc::ClientContext& context) {
    retry->clone()->Setup(context);
    backoff->clone()->Setup(context);
  };
  auto setup_poll = [polling](grpc::ClientContext& context) {
    polling->clone()->Setup(context);
  };
  return Options{}
      .set<internal::GrpcSetupOption>(std::move(setup))
      .set<internal::GrpcSetupPollOption>(std::move(setup_poll));
}

Options MakeInstanceAdminOptions(
    std::shared_ptr<bigtable::RPCRetryPolicy> const& retry,
    std::shared_ptr<bigtable::RPCBackoffPolicy> const& backoff,
    std::shared_ptr<bigtable::PollingPolicy> const& polling) {
  return MakeGrpcSetupOptions(retry, backoff, polling)
      .set<bigtable_admin::BigtableInstanceAdminRetryPolicyOption>(
          MakeCommonRetryPolicy<google::cloud::RetryPolicy>(retry->clone()))
      .set<bigtable_admin::BigtableInstanceAdminBackoffPolicyOption>(
          MakeCommonBackoffPolicy(backoff->clone()))
      .set<bigtable_admin::BigtableInstanceAdminPollingPolicyOption>(
          MakeCommonPollingPolicy(polling->clone()));
}

Options MakeTableAdminOptions(
    std::shared_ptr<bigtable::RPCRetryPolicy> const& retry,
    std::shared_ptr<bigtable::RPCBackoffPolicy> const& backoff,
    std::shared_ptr<bigtable::PollingPolicy> const& polling) {
  return MakeGrpcSetupOptions(retry, backoff, polling)
      .set<bigtable_admin::BigtableTableAdminRetryPolicyOption>(
          MakeCommonRetryPolicy<google::cloud::RetryPolicy>(retry->clone()))
      .set<bigtable_admin::BigtableTableAdminBackoffPolicyOption>(
          MakeCommonBackoffPolicy(backoff->clone()))
      .set<bigtable_admin::BigtableTableAdminPollingPolicyOption>(
          MakeCommonPollingPolicy(polling->clone()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
