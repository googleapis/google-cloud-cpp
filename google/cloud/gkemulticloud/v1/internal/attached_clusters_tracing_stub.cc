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
// source: google/cloud/gkemulticloud/v1/attached_service.proto

#include "google/cloud/gkemulticloud/v1/internal/attached_clusters_tracing_stub.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace gkemulticloud_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

AttachedClustersTracingStub::AttachedClustersTracingStub(
    std::shared_ptr<AttachedClustersStub> child)
    : child_(std::move(child)), propagator_(internal::MakePropagator()) {}

future<StatusOr<google::longrunning::Operation>>
AttachedClustersTracingStub::AsyncCreateAttachedCluster(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::gkemulticloud::v1::CreateAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "CreateAttachedCluster");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncCreateAttachedCluster(cq, context, std::move(options),
                                              request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation>
AttachedClustersTracingStub::CreateAttachedCluster(
    grpc::ClientContext& context, Options options,
    google::cloud::gkemulticloud::v1::CreateAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "CreateAttachedCluster");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->CreateAttachedCluster(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AttachedClustersTracingStub::AsyncUpdateAttachedCluster(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::gkemulticloud::v1::UpdateAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "UpdateAttachedCluster");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncUpdateAttachedCluster(cq, context, std::move(options),
                                              request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation>
AttachedClustersTracingStub::UpdateAttachedCluster(
    grpc::ClientContext& context, Options options,
    google::cloud::gkemulticloud::v1::UpdateAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "UpdateAttachedCluster");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->UpdateAttachedCluster(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AttachedClustersTracingStub::AsyncImportAttachedCluster(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::gkemulticloud::v1::ImportAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "ImportAttachedCluster");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncImportAttachedCluster(cq, context, std::move(options),
                                              request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation>
AttachedClustersTracingStub::ImportAttachedCluster(
    grpc::ClientContext& context, Options options,
    google::cloud::gkemulticloud::v1::ImportAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "ImportAttachedCluster");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->ImportAttachedCluster(context, options, request));
}

StatusOr<google::cloud::gkemulticloud::v1::AttachedCluster>
AttachedClustersTracingStub::GetAttachedCluster(
    grpc::ClientContext& context, Options const& options,
    google::cloud::gkemulticloud::v1::GetAttachedClusterRequest const&
        request) {
  auto span = internal::MakeSpanGrpc(
      "google.cloud.gkemulticloud.v1.AttachedClusters", "GetAttachedCluster");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->GetAttachedCluster(context, options, request));
}

StatusOr<google::cloud::gkemulticloud::v1::ListAttachedClustersResponse>
AttachedClustersTracingStub::ListAttachedClusters(
    grpc::ClientContext& context, Options const& options,
    google::cloud::gkemulticloud::v1::ListAttachedClustersRequest const&
        request) {
  auto span = internal::MakeSpanGrpc(
      "google.cloud.gkemulticloud.v1.AttachedClusters", "ListAttachedClusters");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->ListAttachedClusters(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AttachedClustersTracingStub::AsyncDeleteAttachedCluster(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::gkemulticloud::v1::DeleteAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "DeleteAttachedCluster");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncDeleteAttachedCluster(cq, context, std::move(options),
                                              request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation>
AttachedClustersTracingStub::DeleteAttachedCluster(
    grpc::ClientContext& context, Options options,
    google::cloud::gkemulticloud::v1::DeleteAttachedClusterRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "DeleteAttachedCluster");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->DeleteAttachedCluster(context, options, request));
}

StatusOr<google::cloud::gkemulticloud::v1::AttachedServerConfig>
AttachedClustersTracingStub::GetAttachedServerConfig(
    grpc::ClientContext& context, Options const& options,
    google::cloud::gkemulticloud::v1::GetAttachedServerConfigRequest const&
        request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "GetAttachedServerConfig");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span,
      child_->GetAttachedServerConfig(context, options, request));
}

StatusOr<google::cloud::gkemulticloud::v1::
             GenerateAttachedClusterInstallManifestResponse>
AttachedClustersTracingStub::GenerateAttachedClusterInstallManifest(
    grpc::ClientContext& context, Options const& options,
    google::cloud::gkemulticloud::v1::
        GenerateAttachedClusterInstallManifestRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "GenerateAttachedClusterInstallManifest");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GenerateAttachedClusterInstallManifest(
                               context, options, request));
}

StatusOr<
    google::cloud::gkemulticloud::v1::GenerateAttachedClusterAgentTokenResponse>
AttachedClustersTracingStub::GenerateAttachedClusterAgentToken(
    grpc::ClientContext& context, Options const& options,
    google::cloud::gkemulticloud::v1::
        GenerateAttachedClusterAgentTokenRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.gkemulticloud.v1.AttachedClusters",
                             "GenerateAttachedClusterAgentToken");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span,
      child_->GenerateAttachedClusterAgentToken(context, options, request));
}

StatusOr<google::longrunning::ListOperationsResponse>
AttachedClustersTracingStub::ListOperations(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::ListOperationsRequest const& request) {
  auto span = internal::MakeSpanGrpc(
      "google.cloud.gkemulticloud.v1.AttachedClusters", "ListOperations");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListOperations(context, options, request));
}

StatusOr<google::longrunning::Operation>
AttachedClustersTracingStub::GetOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::GetOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc(
      "google.cloud.gkemulticloud.v1.AttachedClusters", "GetOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetOperation(context, options, request));
}

Status AttachedClustersTracingStub::DeleteOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::DeleteOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc(
      "google.cloud.gkemulticloud.v1.AttachedClusters", "DeleteOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->DeleteOperation(context, options, request));
}

Status AttachedClustersTracingStub::CancelOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::CancelOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc(
      "google.cloud.gkemulticloud.v1.AttachedClusters", "CancelOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->CancelOperation(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AttachedClustersTracingStub::AsyncGetOperation(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::longrunning::GetOperationRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.longrunning.Operations", "GetOperation");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncGetOperation(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

future<Status> AttachedClustersTracingStub::AsyncCancelOperation(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::longrunning::CancelOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.longrunning.Operations",
                                     "CancelOperation");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f =
      child_->AsyncCancelOperation(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::shared_ptr<AttachedClustersStub> MakeAttachedClustersTracingStub(
    std::shared_ptr<AttachedClustersStub> stub) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return std::make_shared<AttachedClustersTracingStub>(std::move(stub));
#else
  return stub;
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace gkemulticloud_v1_internal
}  // namespace cloud
}  // namespace google
