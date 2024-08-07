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
// source: google/cloud/compute/storage_pools/v1/storage_pools.proto

#include "google/cloud/compute/storage_pools/v1/internal/storage_pools_tracing_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/traced_stream_range.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace compute_storage_pools_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

StoragePoolsTracingConnection::StoragePoolsTracingConnection(
    std::shared_ptr<compute_storage_pools_v1::StoragePoolsConnection> child)
    : child_(std::move(child)) {}

StreamRange<std::pair<std::string,
                      google::cloud::cpp::compute::v1::StoragePoolsScopedList>>
StoragePoolsTracingConnection::AggregatedListStoragePools(
    google::cloud::cpp::compute::storage_pools::v1::
        AggregatedListStoragePoolsRequest request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::"
      "AggregatedListStoragePools");
  internal::OTelScope scope(span);
  auto sr = child_->AggregatedListStoragePools(std::move(request));
  return internal::MakeTracedStreamRange<std::pair<
      std::string, google::cloud::cpp::compute::v1::StoragePoolsScopedList>>(
      std::move(span), std::move(sr));
}

future<StatusOr<google::cloud::cpp::compute::v1::Operation>>
StoragePoolsTracingConnection::DeleteStoragePool(
    google::cloud::cpp::compute::storage_pools::v1::
        DeleteStoragePoolRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::DeleteStoragePool");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->DeleteStoragePool(request));
}

StatusOr<google::cloud::cpp::compute::v1::Operation>
StoragePoolsTracingConnection::DeleteStoragePool(
    NoAwaitTag, google::cloud::cpp::compute::storage_pools::v1::
                    DeleteStoragePoolRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::DeleteStoragePool");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->DeleteStoragePool(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::cpp::compute::v1::Operation>>
StoragePoolsTracingConnection::DeleteStoragePool(
    google::cloud::cpp::compute::v1::Operation const& operation) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::DeleteStoragePool");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->DeleteStoragePool(operation));
}

StatusOr<google::cloud::cpp::compute::v1::StoragePool>
StoragePoolsTracingConnection::GetStoragePool(
    google::cloud::cpp::compute::storage_pools::v1::GetStoragePoolRequest const&
        request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::GetStoragePool");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetStoragePool(request));
}

StatusOr<google::cloud::cpp::compute::v1::Policy>
StoragePoolsTracingConnection::GetIamPolicy(
    google::cloud::cpp::compute::storage_pools::v1::GetIamPolicyRequest const&
        request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::GetIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->GetIamPolicy(request));
}

future<StatusOr<google::cloud::cpp::compute::v1::Operation>>
StoragePoolsTracingConnection::InsertStoragePool(
    google::cloud::cpp::compute::storage_pools::v1::
        InsertStoragePoolRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::InsertStoragePool");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->InsertStoragePool(request));
}

StatusOr<google::cloud::cpp::compute::v1::Operation>
StoragePoolsTracingConnection::InsertStoragePool(
    NoAwaitTag, google::cloud::cpp::compute::storage_pools::v1::
                    InsertStoragePoolRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::InsertStoragePool");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->InsertStoragePool(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::cpp::compute::v1::Operation>>
StoragePoolsTracingConnection::InsertStoragePool(
    google::cloud::cpp::compute::v1::Operation const& operation) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::InsertStoragePool");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->InsertStoragePool(operation));
}

StreamRange<google::cloud::cpp::compute::v1::StoragePool>
StoragePoolsTracingConnection::ListStoragePools(
    google::cloud::cpp::compute::storage_pools::v1::ListStoragePoolsRequest
        request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::ListStoragePools");
  internal::OTelScope scope(span);
  auto sr = child_->ListStoragePools(std::move(request));
  return internal::MakeTracedStreamRange<
      google::cloud::cpp::compute::v1::StoragePool>(std::move(span),
                                                    std::move(sr));
}

StreamRange<google::cloud::cpp::compute::v1::StoragePoolDisk>
StoragePoolsTracingConnection::ListDisks(
    google::cloud::cpp::compute::storage_pools::v1::ListDisksRequest request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::ListDisks");
  internal::OTelScope scope(span);
  auto sr = child_->ListDisks(std::move(request));
  return internal::MakeTracedStreamRange<
      google::cloud::cpp::compute::v1::StoragePoolDisk>(std::move(span),
                                                        std::move(sr));
}

StatusOr<google::cloud::cpp::compute::v1::Policy>
StoragePoolsTracingConnection::SetIamPolicy(
    google::cloud::cpp::compute::storage_pools::v1::SetIamPolicyRequest const&
        request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::SetIamPolicy");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->SetIamPolicy(request));
}

StatusOr<google::cloud::cpp::compute::v1::TestPermissionsResponse>
StoragePoolsTracingConnection::TestIamPermissions(
    google::cloud::cpp::compute::storage_pools::v1::
        TestIamPermissionsRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::TestIamPermissions");
  auto scope = opentelemetry::trace::Scope(span);
  return internal::EndSpan(*span, child_->TestIamPermissions(request));
}

future<StatusOr<google::cloud::cpp::compute::v1::Operation>>
StoragePoolsTracingConnection::UpdateStoragePool(
    google::cloud::cpp::compute::storage_pools::v1::
        UpdateStoragePoolRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::UpdateStoragePool");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span), child_->UpdateStoragePool(request));
}

StatusOr<google::cloud::cpp::compute::v1::Operation>
StoragePoolsTracingConnection::UpdateStoragePool(
    NoAwaitTag, google::cloud::cpp::compute::storage_pools::v1::
                    UpdateStoragePoolRequest const& request) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::UpdateStoragePool");
  opentelemetry::trace::Scope scope(span);
  return internal::EndSpan(*span,
                           child_->UpdateStoragePool(NoAwaitTag{}, request));
}

future<StatusOr<google::cloud::cpp::compute::v1::Operation>>
StoragePoolsTracingConnection::UpdateStoragePool(
    google::cloud::cpp::compute::v1::Operation const& operation) {
  auto span = internal::MakeSpan(
      "compute_storage_pools_v1::StoragePoolsConnection::UpdateStoragePool");
  internal::OTelScope scope(span);
  return internal::EndSpan(std::move(span),
                           child_->UpdateStoragePool(operation));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<compute_storage_pools_v1::StoragePoolsConnection>
MakeStoragePoolsTracingConnection(
    std::shared_ptr<compute_storage_pools_v1::StoragePoolsConnection> conn) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (internal::TracingEnabled(conn->options())) {
    conn = std::make_shared<StoragePoolsTracingConnection>(std::move(conn));
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return conn;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace compute_storage_pools_v1_internal
}  // namespace cloud
}  // namespace google
