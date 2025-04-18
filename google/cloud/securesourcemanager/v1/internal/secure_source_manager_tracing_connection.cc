// Copyright 2023 Google LLC
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
// source: google/cloud/securesourcemanager/v1/secure_source_manager.proto

#include "google/cloud/securesourcemanager/v1/internal/secure_source_manager_tracing_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/traced_stream_range.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace securesourcemanager_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

SecureSourceManagerTracingConnection::SecureSourceManagerTracingConnection(
    std::shared_ptr<securesourcemanager_v1::SecureSourceManagerConnection>
        child)
    : child_(std::move(child)) {}

StreamRange<google::cloud::securesourcemanager::v1::Instance>
SecureSourceManagerTracingConnection::ListInstances(
    google::cloud::securesourcemanager::v1::ListInstancesRequest request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::ListInstances");
  internal::OTelScope scope(span);
  auto sr = child_->ListInstances(std::move(request));
  return internal::MakeTracedStreamRange<
      google::cloud::securesourcemanager::v1::Instance>(std::move(span),
                                                        std::move(sr));
}

StatusOr<google::cloud::securesourcemanager::v1::Instance>
SecureSourceManagerTracingConnection::GetInstance(
    google::cloud::securesourcemanager::v1::GetInstanceRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::GetInstance");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetInstance(request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::Instance>>
SecureSourceManagerTracingConnection::CreateInstance(
    google::cloud::securesourcemanager::v1::CreateInstanceRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::CreateInstance");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->CreateInstance(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::CreateInstance(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::CreateInstanceRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::CreateInstance");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->CreateInstance(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::Instance>>
SecureSourceManagerTracingConnection::CreateInstance(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::CreateInstance");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->CreateInstance(operation));
}

future<StatusOr<google::cloud::securesourcemanager::v1::OperationMetadata>>
SecureSourceManagerTracingConnection::DeleteInstance(
    google::cloud::securesourcemanager::v1::DeleteInstanceRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::DeleteInstance");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->DeleteInstance(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::DeleteInstance(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::DeleteInstanceRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::DeleteInstance");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->DeleteInstance(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::OperationMetadata>>
SecureSourceManagerTracingConnection::DeleteInstance(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::DeleteInstance");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->DeleteInstance(operation));
}

StreamRange<google::cloud::securesourcemanager::v1::Repository>
SecureSourceManagerTracingConnection::ListRepositories(
    google::cloud::securesourcemanager::v1::ListRepositoriesRequest request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "ListRepositories");
  internal::OTelScope scope(span);
  auto sr = child_->ListRepositories(std::move(request));
  return internal::MakeTracedStreamRange<
      google::cloud::securesourcemanager::v1::Repository>(std::move(span),
                                                          std::move(sr));
}

StatusOr<google::cloud::securesourcemanager::v1::Repository>
SecureSourceManagerTracingConnection::GetRepository(
    google::cloud::securesourcemanager::v1::GetRepositoryRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::GetRepository");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetRepository(request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::Repository>>
SecureSourceManagerTracingConnection::CreateRepository(
    google::cloud::securesourcemanager::v1::CreateRepositoryRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "CreateRepository");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->CreateRepository(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::CreateRepository(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::CreateRepositoryRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "CreateRepository");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->CreateRepository(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::Repository>>
SecureSourceManagerTracingConnection::CreateRepository(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "CreateRepository");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->CreateRepository(operation));
}

future<StatusOr<google::cloud::securesourcemanager::v1::OperationMetadata>>
SecureSourceManagerTracingConnection::DeleteRepository(
    google::cloud::securesourcemanager::v1::DeleteRepositoryRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "DeleteRepository");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->DeleteRepository(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::DeleteRepository(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::DeleteRepositoryRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "DeleteRepository");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->DeleteRepository(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::OperationMetadata>>
SecureSourceManagerTracingConnection::DeleteRepository(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "DeleteRepository");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->DeleteRepository(operation));
}

StatusOr<google::iam::v1::Policy>
SecureSourceManagerTracingConnection::GetIamPolicyRepo(
    google::iam::v1::GetIamPolicyRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "GetIamPolicyRepo");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetIamPolicyRepo(request));
}

StatusOr<google::iam::v1::Policy>
SecureSourceManagerTracingConnection::SetIamPolicyRepo(
    google::iam::v1::SetIamPolicyRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "SetIamPolicyRepo");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->SetIamPolicyRepo(request));
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
SecureSourceManagerTracingConnection::TestIamPermissionsRepo(
    google::iam::v1::TestIamPermissionsRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "TestIamPermissionsRepo");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->TestIamPermissionsRepo(request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::BranchRule>>
SecureSourceManagerTracingConnection::CreateBranchRule(
    google::cloud::securesourcemanager::v1::CreateBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "CreateBranchRule");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->CreateBranchRule(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::CreateBranchRule(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::CreateBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "CreateBranchRule");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->CreateBranchRule(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::BranchRule>>
SecureSourceManagerTracingConnection::CreateBranchRule(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "CreateBranchRule");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->CreateBranchRule(operation));
}

StreamRange<google::cloud::securesourcemanager::v1::BranchRule>
SecureSourceManagerTracingConnection::ListBranchRules(
    google::cloud::securesourcemanager::v1::ListBranchRulesRequest request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::ListBranchRules");
  internal::OTelScope scope(span);
  auto sr = child_->ListBranchRules(std::move(request));
  return internal::MakeTracedStreamRange<
      google::cloud::securesourcemanager::v1::BranchRule>(std::move(span),
                                                          std::move(sr));
}

StatusOr<google::cloud::securesourcemanager::v1::BranchRule>
SecureSourceManagerTracingConnection::GetBranchRule(
    google::cloud::securesourcemanager::v1::GetBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::GetBranchRule");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetBranchRule(request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::BranchRule>>
SecureSourceManagerTracingConnection::UpdateBranchRule(
    google::cloud::securesourcemanager::v1::UpdateBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "UpdateBranchRule");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->UpdateBranchRule(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::UpdateBranchRule(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::UpdateBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "UpdateBranchRule");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->UpdateBranchRule(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::BranchRule>>
SecureSourceManagerTracingConnection::UpdateBranchRule(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "UpdateBranchRule");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->UpdateBranchRule(operation));
}

future<StatusOr<google::cloud::securesourcemanager::v1::OperationMetadata>>
SecureSourceManagerTracingConnection::DeleteBranchRule(
    google::cloud::securesourcemanager::v1::DeleteBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "DeleteBranchRule");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->DeleteBranchRule(request));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::DeleteBranchRule(
    NoAwaitTag,
    google::cloud::securesourcemanager::v1::DeleteBranchRuleRequest const&
        request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "DeleteBranchRule");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->DeleteBranchRule(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::securesourcemanager::v1::OperationMetadata>>
SecureSourceManagerTracingConnection::DeleteBranchRule(
    google::longrunning::Operation const& operation) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "DeleteBranchRule");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->DeleteBranchRule(operation));
}

StreamRange<google::cloud::location::Location>
SecureSourceManagerTracingConnection::ListLocations(
    google::cloud::location::ListLocationsRequest request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::ListLocations");
  internal::OTelScope scope(span);
  auto sr = child_->ListLocations(std::move(request));
  return internal::MakeTracedStreamRange<google::cloud::location::Location>(
      std::move(span), std::move(sr));
}

StatusOr<google::cloud::location::Location>
SecureSourceManagerTracingConnection::GetLocation(
    google::cloud::location::GetLocationRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::GetLocation");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetLocation(request));
}

StatusOr<google::iam::v1::Policy>
SecureSourceManagerTracingConnection::SetIamPolicy(
    google::iam::v1::SetIamPolicyRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::SetIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->SetIamPolicy(request));
}

StatusOr<google::iam::v1::Policy>
SecureSourceManagerTracingConnection::GetIamPolicy(
    google::iam::v1::GetIamPolicyRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::GetIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetIamPolicy(request));
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
SecureSourceManagerTracingConnection::TestIamPermissions(
    google::iam::v1::TestIamPermissionsRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::"
      "TestIamPermissions");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->TestIamPermissions(request));
}

StreamRange<google::longrunning::Operation>
SecureSourceManagerTracingConnection::ListOperations(
    google::longrunning::ListOperationsRequest request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::ListOperations");
  internal::OTelScope scope(span);
  auto sr = child_->ListOperations(std::move(request));
  return internal::MakeTracedStreamRange<google::longrunning::Operation>(
      std::move(span), std::move(sr));
}

StatusOr<google::longrunning::Operation>
SecureSourceManagerTracingConnection::GetOperation(
    google::longrunning::GetOperationRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::GetOperation");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetOperation(request));
}

Status SecureSourceManagerTracingConnection::DeleteOperation(
    google::longrunning::DeleteOperationRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::DeleteOperation");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->DeleteOperation(request));
}

Status SecureSourceManagerTracingConnection::CancelOperation(
    google::longrunning::CancelOperationRequest const& request) {
  auto span = internal::MakeSpan(
      "securesourcemanager_v1::SecureSourceManagerConnection::CancelOperation");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->CancelOperation(request));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<securesourcemanager_v1::SecureSourceManagerConnection>
MakeSecureSourceManagerTracingConnection(
    std::shared_ptr<securesourcemanager_v1::SecureSourceManagerConnection>
        conn) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (internal::TracingEnabled(conn->options())) {
    conn =
        std::make_shared<SecureSourceManagerTracingConnection>(std::move(conn));
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return conn;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace securesourcemanager_v1_internal
}  // namespace cloud
}  // namespace google
