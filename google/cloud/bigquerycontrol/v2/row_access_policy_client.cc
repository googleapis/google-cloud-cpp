// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/bigquery/v2/row_access_policy.proto

#include "google/cloud/bigquerycontrol/v2/row_access_policy_client.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace bigquerycontrol_v2 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RowAccessPolicyServiceClient::RowAccessPolicyServiceClient(
    std::shared_ptr<RowAccessPolicyServiceConnection> connection, Options opts)
    : connection_(std::move(connection)),
      options_(
          internal::MergeOptions(std::move(opts), connection_->options())) {}
RowAccessPolicyServiceClient::~RowAccessPolicyServiceClient() = default;

StreamRange<google::cloud::bigquery::v2::RowAccessPolicy>
RowAccessPolicyServiceClient::ListRowAccessPolicies(
    google::cloud::bigquery::v2::ListRowAccessPoliciesRequest request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ListRowAccessPolicies(std::move(request));
}

StatusOr<google::cloud::bigquery::v2::RowAccessPolicy>
RowAccessPolicyServiceClient::GetRowAccessPolicy(
    google::cloud::bigquery::v2::GetRowAccessPolicyRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GetRowAccessPolicy(request);
}

StatusOr<google::cloud::bigquery::v2::RowAccessPolicy>
RowAccessPolicyServiceClient::CreateRowAccessPolicy(
    google::cloud::bigquery::v2::CreateRowAccessPolicyRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CreateRowAccessPolicy(request);
}

StatusOr<google::cloud::bigquery::v2::RowAccessPolicy>
RowAccessPolicyServiceClient::UpdateRowAccessPolicy(
    google::cloud::bigquery::v2::UpdateRowAccessPolicyRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->UpdateRowAccessPolicy(request);
}

Status RowAccessPolicyServiceClient::DeleteRowAccessPolicy(
    google::cloud::bigquery::v2::DeleteRowAccessPolicyRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->DeleteRowAccessPolicy(request);
}

Status RowAccessPolicyServiceClient::BatchDeleteRowAccessPolicies(
    google::cloud::bigquery::v2::BatchDeleteRowAccessPoliciesRequest const&
        request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->BatchDeleteRowAccessPolicies(request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquerycontrol_v2
}  // namespace cloud
}  // namespace google
