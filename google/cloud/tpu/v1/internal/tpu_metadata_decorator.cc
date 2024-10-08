// Copyright 2022 Google LLC
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
// source: google/cloud/tpu/v1/cloud_tpu.proto

#include "google/cloud/tpu/v1/internal/tpu_metadata_decorator.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/url_encode.h"
#include "google/cloud/status_or.h"
#include <google/cloud/tpu/v1/cloud_tpu.grpc.pb.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace tpu_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TpuMetadata::TpuMetadata(std::shared_ptr<TpuStub> child,
                         std::multimap<std::string, std::string> fixed_metadata,
                         std::string api_client_header)
    : child_(std::move(child)),
      fixed_metadata_(std::move(fixed_metadata)),
      api_client_header_(
          api_client_header.empty()
              ? google::cloud::internal::GeneratedLibClientHeader()
              : std::move(api_client_header)) {}

StatusOr<google::cloud::tpu::v1::ListNodesResponse> TpuMetadata::ListNodes(
    grpc::ClientContext& context, Options const& options,
    google::cloud::tpu::v1::ListNodesRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("parent=", internal::UrlEncode(request.parent())));
  return child_->ListNodes(context, options, request);
}

StatusOr<google::cloud::tpu::v1::Node> TpuMetadata::GetNode(
    grpc::ClientContext& context, Options const& options,
    google::cloud::tpu::v1::GetNodeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->GetNode(context, options, request);
}

future<StatusOr<google::longrunning::Operation>> TpuMetadata::AsyncCreateNode(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::tpu::v1::CreateNodeRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("parent=", internal::UrlEncode(request.parent())));
  return child_->AsyncCreateNode(cq, std::move(context), std::move(options),
                                 request);
}

StatusOr<google::longrunning::Operation> TpuMetadata::CreateNode(
    grpc::ClientContext& context, Options options,
    google::cloud::tpu::v1::CreateNodeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("parent=", internal::UrlEncode(request.parent())));
  return child_->CreateNode(context, options, request);
}

future<StatusOr<google::longrunning::Operation>> TpuMetadata::AsyncDeleteNode(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::tpu::v1::DeleteNodeRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->AsyncDeleteNode(cq, std::move(context), std::move(options),
                                 request);
}

StatusOr<google::longrunning::Operation> TpuMetadata::DeleteNode(
    grpc::ClientContext& context, Options options,
    google::cloud::tpu::v1::DeleteNodeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->DeleteNode(context, options, request);
}

future<StatusOr<google::longrunning::Operation>> TpuMetadata::AsyncReimageNode(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::tpu::v1::ReimageNodeRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->AsyncReimageNode(cq, std::move(context), std::move(options),
                                  request);
}

StatusOr<google::longrunning::Operation> TpuMetadata::ReimageNode(
    grpc::ClientContext& context, Options options,
    google::cloud::tpu::v1::ReimageNodeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->ReimageNode(context, options, request);
}

future<StatusOr<google::longrunning::Operation>> TpuMetadata::AsyncStopNode(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::tpu::v1::StopNodeRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->AsyncStopNode(cq, std::move(context), std::move(options),
                               request);
}

StatusOr<google::longrunning::Operation> TpuMetadata::StopNode(
    grpc::ClientContext& context, Options options,
    google::cloud::tpu::v1::StopNodeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->StopNode(context, options, request);
}

future<StatusOr<google::longrunning::Operation>> TpuMetadata::AsyncStartNode(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::tpu::v1::StartNodeRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->AsyncStartNode(cq, std::move(context), std::move(options),
                                request);
}

StatusOr<google::longrunning::Operation> TpuMetadata::StartNode(
    grpc::ClientContext& context, Options options,
    google::cloud::tpu::v1::StartNodeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->StartNode(context, options, request);
}

StatusOr<google::cloud::tpu::v1::ListTensorFlowVersionsResponse>
TpuMetadata::ListTensorFlowVersions(
    grpc::ClientContext& context, Options const& options,
    google::cloud::tpu::v1::ListTensorFlowVersionsRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("parent=", internal::UrlEncode(request.parent())));
  return child_->ListTensorFlowVersions(context, options, request);
}

StatusOr<google::cloud::tpu::v1::TensorFlowVersion>
TpuMetadata::GetTensorFlowVersion(
    grpc::ClientContext& context, Options const& options,
    google::cloud::tpu::v1::GetTensorFlowVersionRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->GetTensorFlowVersion(context, options, request);
}

StatusOr<google::cloud::tpu::v1::ListAcceleratorTypesResponse>
TpuMetadata::ListAcceleratorTypes(
    grpc::ClientContext& context, Options const& options,
    google::cloud::tpu::v1::ListAcceleratorTypesRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("parent=", internal::UrlEncode(request.parent())));
  return child_->ListAcceleratorTypes(context, options, request);
}

StatusOr<google::cloud::tpu::v1::AcceleratorType>
TpuMetadata::GetAcceleratorType(
    grpc::ClientContext& context, Options const& options,
    google::cloud::tpu::v1::GetAcceleratorTypeRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->GetAcceleratorType(context, options, request);
}

StatusOr<google::cloud::location::ListLocationsResponse>
TpuMetadata::ListLocations(
    grpc::ClientContext& context, Options const& options,
    google::cloud::location::ListLocationsRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->ListLocations(context, options, request);
}

StatusOr<google::cloud::location::Location> TpuMetadata::GetLocation(
    grpc::ClientContext& context, Options const& options,
    google::cloud::location::GetLocationRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->GetLocation(context, options, request);
}

StatusOr<google::longrunning::ListOperationsResponse>
TpuMetadata::ListOperations(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::ListOperationsRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->ListOperations(context, options, request);
}

StatusOr<google::longrunning::Operation> TpuMetadata::GetOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::GetOperationRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->GetOperation(context, options, request);
}

Status TpuMetadata::DeleteOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::DeleteOperationRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->DeleteOperation(context, options, request);
}

Status TpuMetadata::CancelOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::CancelOperationRequest const& request) {
  SetMetadata(context, options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->CancelOperation(context, options, request);
}

future<StatusOr<google::longrunning::Operation>> TpuMetadata::AsyncGetOperation(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::longrunning::GetOperationRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->AsyncGetOperation(cq, std::move(context), std::move(options),
                                   request);
}

future<Status> TpuMetadata::AsyncCancelOperation(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::longrunning::CancelOperationRequest const& request) {
  SetMetadata(*context, *options,
              absl::StrCat("name=", internal::UrlEncode(request.name())));
  return child_->AsyncCancelOperation(cq, std::move(context),
                                      std::move(options), request);
}

void TpuMetadata::SetMetadata(grpc::ClientContext& context,
                              Options const& options,
                              std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  SetMetadata(context, options);
}

void TpuMetadata::SetMetadata(grpc::ClientContext& context,
                              Options const& options) {
  google::cloud::internal::SetMetadata(context, options, fixed_metadata_,
                                       api_client_header_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace tpu_v1_internal
}  // namespace cloud
}  // namespace google
