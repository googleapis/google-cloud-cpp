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
// source: google/cloud/aiplatform/v1/metadata_service.proto

#include "google/cloud/aiplatform/v1/internal/metadata_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status_or.h"
#include <google/cloud/aiplatform/v1/metadata_service.grpc.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace aiplatform_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

MetadataServiceStub::~MetadataServiceStub() = default;

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncCreateMetadataStore(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::CreateMetadataStoreRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::CreateMetadataStoreRequest,
      google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::cloud::aiplatform::v1::CreateMetadataStoreRequest const&
                 request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncCreateMetadataStore(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::CreateMetadataStore(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::CreateMetadataStoreRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->CreateMetadataStore(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::MetadataStore>
DefaultMetadataServiceStub::GetMetadataStore(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::GetMetadataStoreRequest const& request) {
  google::cloud::aiplatform::v1::MetadataStore response;
  auto status = grpc_stub_->GetMetadataStore(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::ListMetadataStoresResponse>
DefaultMetadataServiceStub::ListMetadataStores(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::ListMetadataStoresRequest const& request) {
  google::cloud::aiplatform::v1::ListMetadataStoresResponse response;
  auto status = grpc_stub_->ListMetadataStores(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncDeleteMetadataStore(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::DeleteMetadataStoreRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::DeleteMetadataStoreRequest,
      google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::cloud::aiplatform::v1::DeleteMetadataStoreRequest const&
                 request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncDeleteMetadataStore(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::DeleteMetadataStore(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::DeleteMetadataStoreRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->DeleteMetadataStore(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Artifact>
DefaultMetadataServiceStub::CreateArtifact(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::CreateArtifactRequest const& request) {
  google::cloud::aiplatform::v1::Artifact response;
  auto status = grpc_stub_->CreateArtifact(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Artifact>
DefaultMetadataServiceStub::GetArtifact(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::GetArtifactRequest const& request) {
  google::cloud::aiplatform::v1::Artifact response;
  auto status = grpc_stub_->GetArtifact(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::ListArtifactsResponse>
DefaultMetadataServiceStub::ListArtifacts(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::ListArtifactsRequest const& request) {
  google::cloud::aiplatform::v1::ListArtifactsResponse response;
  auto status = grpc_stub_->ListArtifacts(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Artifact>
DefaultMetadataServiceStub::UpdateArtifact(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::UpdateArtifactRequest const& request) {
  google::cloud::aiplatform::v1::Artifact response;
  auto status = grpc_stub_->UpdateArtifact(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncDeleteArtifact(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::DeleteArtifactRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::DeleteArtifactRequest,
      google::longrunning::Operation>(
      cq,
      [this](
          grpc::ClientContext* context,
          google::cloud::aiplatform::v1::DeleteArtifactRequest const& request,
          grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncDeleteArtifact(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::DeleteArtifact(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::DeleteArtifactRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->DeleteArtifact(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncPurgeArtifacts(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::PurgeArtifactsRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::PurgeArtifactsRequest,
      google::longrunning::Operation>(
      cq,
      [this](
          grpc::ClientContext* context,
          google::cloud::aiplatform::v1::PurgeArtifactsRequest const& request,
          grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncPurgeArtifacts(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::PurgeArtifacts(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::PurgeArtifactsRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->PurgeArtifacts(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Context>
DefaultMetadataServiceStub::CreateContext(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::CreateContextRequest const& request) {
  google::cloud::aiplatform::v1::Context response;
  auto status = grpc_stub_->CreateContext(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Context>
DefaultMetadataServiceStub::GetContext(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::GetContextRequest const& request) {
  google::cloud::aiplatform::v1::Context response;
  auto status = grpc_stub_->GetContext(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::ListContextsResponse>
DefaultMetadataServiceStub::ListContexts(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::ListContextsRequest const& request) {
  google::cloud::aiplatform::v1::ListContextsResponse response;
  auto status = grpc_stub_->ListContexts(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Context>
DefaultMetadataServiceStub::UpdateContext(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::UpdateContextRequest const& request) {
  google::cloud::aiplatform::v1::Context response;
  auto status = grpc_stub_->UpdateContext(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncDeleteContext(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::DeleteContextRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::DeleteContextRequest,
      google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::cloud::aiplatform::v1::DeleteContextRequest const& request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncDeleteContext(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::DeleteContext(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::DeleteContextRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->DeleteContext(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncPurgeContexts(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::PurgeContextsRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::PurgeContextsRequest,
      google::longrunning::Operation>(
      cq,
      [this](grpc::ClientContext* context,
             google::cloud::aiplatform::v1::PurgeContextsRequest const& request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncPurgeContexts(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::PurgeContexts(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::PurgeContextsRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->PurgeContexts(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<
    google::cloud::aiplatform::v1::AddContextArtifactsAndExecutionsResponse>
DefaultMetadataServiceStub::AddContextArtifactsAndExecutions(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::
        AddContextArtifactsAndExecutionsRequest const& request) {
  google::cloud::aiplatform::v1::AddContextArtifactsAndExecutionsResponse
      response;
  auto status = grpc_stub_->AddContextArtifactsAndExecutions(&context, request,
                                                             &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::AddContextChildrenResponse>
DefaultMetadataServiceStub::AddContextChildren(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::AddContextChildrenRequest const& request) {
  google::cloud::aiplatform::v1::AddContextChildrenResponse response;
  auto status = grpc_stub_->AddContextChildren(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::RemoveContextChildrenResponse>
DefaultMetadataServiceStub::RemoveContextChildren(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::RemoveContextChildrenRequest const&
        request) {
  google::cloud::aiplatform::v1::RemoveContextChildrenResponse response;
  auto status = grpc_stub_->RemoveContextChildren(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::LineageSubgraph>
DefaultMetadataServiceStub::QueryContextLineageSubgraph(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::QueryContextLineageSubgraphRequest const&
        request) {
  google::cloud::aiplatform::v1::LineageSubgraph response;
  auto status =
      grpc_stub_->QueryContextLineageSubgraph(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Execution>
DefaultMetadataServiceStub::CreateExecution(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::CreateExecutionRequest const& request) {
  google::cloud::aiplatform::v1::Execution response;
  auto status = grpc_stub_->CreateExecution(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Execution>
DefaultMetadataServiceStub::GetExecution(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::GetExecutionRequest const& request) {
  google::cloud::aiplatform::v1::Execution response;
  auto status = grpc_stub_->GetExecution(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::ListExecutionsResponse>
DefaultMetadataServiceStub::ListExecutions(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::ListExecutionsRequest const& request) {
  google::cloud::aiplatform::v1::ListExecutionsResponse response;
  auto status = grpc_stub_->ListExecutions(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::Execution>
DefaultMetadataServiceStub::UpdateExecution(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::UpdateExecutionRequest const& request) {
  google::cloud::aiplatform::v1::Execution response;
  auto status = grpc_stub_->UpdateExecution(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncDeleteExecution(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::DeleteExecutionRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::DeleteExecutionRequest,
      google::longrunning::Operation>(
      cq,
      [this](
          grpc::ClientContext* context,
          google::cloud::aiplatform::v1::DeleteExecutionRequest const& request,
          grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncDeleteExecution(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::DeleteExecution(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::DeleteExecutionRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->DeleteExecution(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncPurgeExecutions(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions,
    google::cloud::aiplatform::v1::PurgeExecutionsRequest const& request) {
  return internal::MakeUnaryRpcImpl<
      google::cloud::aiplatform::v1::PurgeExecutionsRequest,
      google::longrunning::Operation>(
      cq,
      [this](
          grpc::ClientContext* context,
          google::cloud::aiplatform::v1::PurgeExecutionsRequest const& request,
          grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncPurgeExecutions(context, request, cq);
      },
      request, std::move(context));
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::PurgeExecutions(
    grpc::ClientContext& context, Options,
    google::cloud::aiplatform::v1::PurgeExecutionsRequest const& request) {
  google::longrunning::Operation response;
  auto status = grpc_stub_->PurgeExecutions(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::AddExecutionEventsResponse>
DefaultMetadataServiceStub::AddExecutionEvents(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::AddExecutionEventsRequest const& request) {
  google::cloud::aiplatform::v1::AddExecutionEventsResponse response;
  auto status = grpc_stub_->AddExecutionEvents(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::LineageSubgraph>
DefaultMetadataServiceStub::QueryExecutionInputsAndOutputs(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::QueryExecutionInputsAndOutputsRequest const&
        request) {
  google::cloud::aiplatform::v1::LineageSubgraph response;
  auto status =
      grpc_stub_->QueryExecutionInputsAndOutputs(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::MetadataSchema>
DefaultMetadataServiceStub::CreateMetadataSchema(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::CreateMetadataSchemaRequest const& request) {
  google::cloud::aiplatform::v1::MetadataSchema response;
  auto status = grpc_stub_->CreateMetadataSchema(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::MetadataSchema>
DefaultMetadataServiceStub::GetMetadataSchema(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::GetMetadataSchemaRequest const& request) {
  google::cloud::aiplatform::v1::MetadataSchema response;
  auto status = grpc_stub_->GetMetadataSchema(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::ListMetadataSchemasResponse>
DefaultMetadataServiceStub::ListMetadataSchemas(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::ListMetadataSchemasRequest const& request) {
  google::cloud::aiplatform::v1::ListMetadataSchemasResponse response;
  auto status = grpc_stub_->ListMetadataSchemas(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::aiplatform::v1::LineageSubgraph>
DefaultMetadataServiceStub::QueryArtifactLineageSubgraph(
    grpc::ClientContext& context, Options const&,
    google::cloud::aiplatform::v1::QueryArtifactLineageSubgraphRequest const&
        request) {
  google::cloud::aiplatform::v1::LineageSubgraph response;
  auto status =
      grpc_stub_->QueryArtifactLineageSubgraph(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::location::ListLocationsResponse>
DefaultMetadataServiceStub::ListLocations(
    grpc::ClientContext& context, Options const&,
    google::cloud::location::ListLocationsRequest const& request) {
  google::cloud::location::ListLocationsResponse response;
  auto status = locations_stub_->ListLocations(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::location::Location>
DefaultMetadataServiceStub::GetLocation(
    grpc::ClientContext& context, Options const&,
    google::cloud::location::GetLocationRequest const& request) {
  google::cloud::location::Location response;
  auto status = locations_stub_->GetLocation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::iam::v1::Policy> DefaultMetadataServiceStub::SetIamPolicy(
    grpc::ClientContext& context, Options const&,
    google::iam::v1::SetIamPolicyRequest const& request) {
  google::iam::v1::Policy response;
  auto status = iampolicy_stub_->SetIamPolicy(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::iam::v1::Policy> DefaultMetadataServiceStub::GetIamPolicy(
    grpc::ClientContext& context, Options const&,
    google::iam::v1::GetIamPolicyRequest const& request) {
  google::iam::v1::Policy response;
  auto status = iampolicy_stub_->GetIamPolicy(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
DefaultMetadataServiceStub::TestIamPermissions(
    grpc::ClientContext& context, Options const&,
    google::iam::v1::TestIamPermissionsRequest const& request) {
  google::iam::v1::TestIamPermissionsResponse response;
  auto status =
      iampolicy_stub_->TestIamPermissions(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::longrunning::ListOperationsResponse>
DefaultMetadataServiceStub::ListOperations(
    grpc::ClientContext& context, Options const&,
    google::longrunning::ListOperationsRequest const& request) {
  google::longrunning::ListOperationsResponse response;
  auto status = operations_stub_->ListOperations(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::GetOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::GetOperationRequest const& request) {
  google::longrunning::Operation response;
  auto status = operations_stub_->GetOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

Status DefaultMetadataServiceStub::DeleteOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::DeleteOperationRequest const& request) {
  google::protobuf::Empty response;
  auto status = operations_stub_->DeleteOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return google::cloud::Status();
}

Status DefaultMetadataServiceStub::CancelOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::CancelOperationRequest const& request) {
  google::protobuf::Empty response;
  auto status = operations_stub_->CancelOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return google::cloud::Status();
}

StatusOr<google::longrunning::Operation>
DefaultMetadataServiceStub::WaitOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::WaitOperationRequest const& request) {
  google::longrunning::Operation response;
  auto status = operations_stub_->WaitOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

future<StatusOr<google::longrunning::Operation>>
DefaultMetadataServiceStub::AsyncGetOperation(
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

future<Status> DefaultMetadataServiceStub::AsyncCancelOperation(
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
}  // namespace aiplatform_v1_internal
}  // namespace cloud
}  // namespace google
