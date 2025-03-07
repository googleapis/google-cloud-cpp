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
// source: google/cloud/resourcemanager/v3/tag_bindings.proto

#include "google/cloud/resourcemanager/v3/internal/tag_bindings_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status_or.h"
#include <google/cloud/resourcemanager/v3/tag_bindings.grpc.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace resourcemanager_v3_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TagBindingsStub::~TagBindingsStub() = default;

StatusOr<google::cloud::resourcemanager::v3::ListTagBindingsResponse>
DefaultTagBindingsStub::ListTagBindings(
    grpc::ClientContext& context, Options const&,
    google::cloud::resourcemanager::v3::ListTagBindingsRequest const& request) {
  google::cloud::resourcemanager::v3::ListTagBindingsResponse response;
  auto status = grpc_stub_->ListTagBindings(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultTagBindingsStub::AsyncCreateTagBinding(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::resourcemanager::v3::CreateTagBindingRequest const&
        request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::resourcemanager::v3::CreateTagBindingRequest,
      google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::cloud::resourcemanager::v3::CreateTagBindingRequest const&
                 request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncCreateTagBinding(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultTagBindingsStub::CreateTagBinding(
    grpc::ClientContext& context, Options,
    google::cloud::resourcemanager::v3::CreateTagBindingRequest const&
        request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->CreateTagBinding(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultTagBindingsStub::AsyncDeleteTagBinding(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::resourcemanager::v3::DeleteTagBindingRequest const&
        request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::resourcemanager::v3::DeleteTagBindingRequest,
      google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::cloud::resourcemanager::v3::DeleteTagBindingRequest const&
                 request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncDeleteTagBinding(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultTagBindingsStub::DeleteTagBinding(
    grpc::ClientContext& context, Options,
    google::cloud::resourcemanager::v3::DeleteTagBindingRequest const&
        request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->DeleteTagBinding(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::resourcemanager::v3::ListEffectiveTagsResponse>
DefaultTagBindingsStub::ListEffectiveTags(
    grpc::ClientContext& context, Options const&,
    google::cloud::resourcemanager::v3::ListEffectiveTagsRequest const&
        request) {
  google::cloud::resourcemanager::v3::ListEffectiveTagsResponse response;
  auto status = grpc_stub_->ListEffectiveTags(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::longrunning::Operation> DefaultTagBindingsStub::GetOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::GetOperationRequest const& request) {
  google::longrunning::Operation response;
  auto status = operations_stub_->GetOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultTagBindingsStub::AsyncGetOperation(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    google::cloud::internal::ImmutableOptions,
    google::longrunning::GetOperationRequest const& request) {
  return internal::MakeUnaryRpcImpl<google::longrunning::GetOperationRequest,
                                    google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::longrunning::GetOperationRequest const& request,
             grpc::CompletionQueue* cq) {
        return operations_stub_->AsyncGetOperation(context, request, cq);
      },
      request, std::move(context));
}

future<Status> DefaultTagBindingsStub::AsyncCancelOperation(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    google::cloud::internal::ImmutableOptions,
    google::longrunning::CancelOperationRequest const& request) {
  return internal::MakeUnaryRpcImpl<google::longrunning::CancelOperationRequest,
                                    google::protobuf::Empty>(
             cq,
             [this](grpc::ClientContext* context,
                    google::longrunning::CancelOperationRequest const& request,
                    grpc::CompletionQueue* cq) {
               return operations_stub_->AsyncCancelOperation(context, request,
                                                             cq);
             },
             request, std::move(context))
      .then([](future<StatusOr<google::protobuf::Empty>> f) {
        return f.get().status();
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace resourcemanager_v3_internal
}  // namespace cloud
}  // namespace google
