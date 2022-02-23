// Copyright 2019 Google LLC
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

// TODO(#7356): Remove this file after the deprecation period expires
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::spanner::admin::instance::v1::Instance>
InstanceAdminClient::GetInstance(Instance const& in) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetInstance({in.FullName()});
}

future<StatusOr<google::spanner::admin::instance::v1::Instance>>
InstanceAdminClient::CreateInstance(
    google::spanner::admin::instance::v1::CreateInstanceRequest const&
        request) {
  internal::OptionsSpan span(conn_->options());
  return conn_->CreateInstance({request});
}

future<StatusOr<google::spanner::admin::instance::v1::Instance>>
InstanceAdminClient::UpdateInstance(
    google::spanner::admin::instance::v1::UpdateInstanceRequest const&
        request) {
  internal::OptionsSpan span(conn_->options());
  return conn_->UpdateInstance({request});
}

Status InstanceAdminClient::DeleteInstance(Instance const& in) {
  internal::OptionsSpan span(conn_->options());
  return conn_->DeleteInstance({in.FullName()});
}

StatusOr<google::spanner::admin::instance::v1::InstanceConfig>
InstanceAdminClient::GetInstanceConfig(std::string const& name) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetInstanceConfig({name});
}

ListInstanceConfigsRange InstanceAdminClient::ListInstanceConfigs(
    std::string project_id) {
  internal::OptionsSpan span(conn_->options());
  return conn_->ListInstanceConfigs({std::move(project_id)});
}

ListInstancesRange InstanceAdminClient::ListInstances(std::string project_id,
                                                      std::string filter) {
  internal::OptionsSpan span(conn_->options());
  return conn_->ListInstances({std::move(project_id), std::move(filter)});
}

StatusOr<google::iam::v1::Policy> InstanceAdminClient::GetIamPolicy(
    Instance const& in) {
  internal::OptionsSpan span(conn_->options());
  return conn_->GetIamPolicy({in.FullName()});
}

StatusOr<google::iam::v1::Policy> InstanceAdminClient::SetIamPolicy(
    Instance const& in, google::iam::v1::Policy policy) {
  internal::OptionsSpan span(conn_->options());
  return conn_->SetIamPolicy({in.FullName(), std::move(policy)});
}

StatusOr<google::iam::v1::Policy> InstanceAdminClient::SetIamPolicy(
    Instance const& in, IamUpdater const& updater) {
  auto const rerun_maximum_duration = std::chrono::minutes(15);
  auto default_rerun_policy =
      LimitedTimeTransactionRerunPolicy(rerun_maximum_duration).clone();

  auto const backoff_initial_delay = std::chrono::milliseconds(1000);
  auto const backoff_maximum_delay = std::chrono::minutes(5);
  auto const backoff_scaling = 2.0;
  auto default_backoff_policy =
      ExponentialBackoffPolicy(backoff_initial_delay, backoff_maximum_delay,
                               backoff_scaling)
          .clone();

  return SetIamPolicy(in, updater, std::move(default_rerun_policy),
                      std::move(default_backoff_policy));
}

StatusOr<google::iam::v1::Policy> InstanceAdminClient::SetIamPolicy(
    Instance const& in, IamUpdater const& updater,
    std::unique_ptr<TransactionRerunPolicy> rerun_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  internal::OptionsSpan span(conn_->options());
  using RerunnablePolicy = spanner_internal::SafeTransactionRerun;

  Status last_status;
  do {
    auto current_policy = conn_->GetIamPolicy({in.FullName()});
    if (!current_policy) {
      last_status = std::move(current_policy).status();
    } else {
      auto etag = current_policy->etag();
      auto desired = updater(*current_policy);
      if (!desired.has_value()) {
        return current_policy;
      }
      desired->set_etag(std::move(etag));
      auto result = conn_->SetIamPolicy({in.FullName(), *std::move(desired)});
      if (RerunnablePolicy::IsOk(result.status())) {
        return result;
      }
      last_status = std::move(result).status();
    }
    if (!rerun_policy->OnFailure(last_status)) break;
    std::this_thread::sleep_for(backoff_policy->OnCompletion());
  } while (!rerun_policy->IsExhausted());
  return last_status;
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
InstanceAdminClient::TestIamPermissions(Instance const& in,
                                        std::vector<std::string> permissions) {
  internal::OptionsSpan span(conn_->options());
  return conn_->TestIamPermissions({in.FullName(), std::move(permissions)});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
