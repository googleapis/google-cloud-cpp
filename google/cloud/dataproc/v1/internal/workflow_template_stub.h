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
// source: google/cloud/dataproc/v1/workflow_templates.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DATAPROC_V1_INTERNAL_WORKFLOW_TEMPLATE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DATAPROC_V1_INTERNAL_WORKFLOW_TEMPLATE_STUB_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/cloud/dataproc/v1/workflow_templates.grpc.pb.h>
#include <google/iam/v1/iam_policy.grpc.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace dataproc_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class WorkflowTemplateServiceStub {
 public:
  virtual ~WorkflowTemplateServiceStub() = 0;

  virtual StatusOr<google::cloud::dataproc::v1::WorkflowTemplate>
  CreateWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::CreateWorkflowTemplateRequest const&
          request) = 0;

  virtual StatusOr<google::cloud::dataproc::v1::WorkflowTemplate>
  GetWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::GetWorkflowTemplateRequest const&
          request) = 0;

  virtual future<StatusOr<google::longrunning::Operation>>
  AsyncInstantiateWorkflowTemplate(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::dataproc::v1::InstantiateWorkflowTemplateRequest const&
          request) = 0;

  virtual StatusOr<google::longrunning::Operation> InstantiateWorkflowTemplate(
      grpc::ClientContext& context, Options options,
      google::cloud::dataproc::v1::InstantiateWorkflowTemplateRequest const&
          request) = 0;

  virtual future<StatusOr<google::longrunning::Operation>>
  AsyncInstantiateInlineWorkflowTemplate(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::dataproc::v1::
          InstantiateInlineWorkflowTemplateRequest const& request) = 0;

  virtual StatusOr<google::longrunning::Operation>
  InstantiateInlineWorkflowTemplate(
      grpc::ClientContext& context, Options options,
      google::cloud::dataproc::v1::
          InstantiateInlineWorkflowTemplateRequest const& request) = 0;

  virtual StatusOr<google::cloud::dataproc::v1::WorkflowTemplate>
  UpdateWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::UpdateWorkflowTemplateRequest const&
          request) = 0;

  virtual StatusOr<google::cloud::dataproc::v1::ListWorkflowTemplatesResponse>
  ListWorkflowTemplates(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::ListWorkflowTemplatesRequest const&
          request) = 0;

  virtual Status DeleteWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::DeleteWorkflowTemplateRequest const&
          request) = 0;

  virtual StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::SetIamPolicyRequest const& request) = 0;

  virtual StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::GetIamPolicyRequest const& request) = 0;

  virtual StatusOr<google::iam::v1::TestIamPermissionsResponse>
  TestIamPermissions(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::TestIamPermissionsRequest const& request) = 0;

  virtual StatusOr<google::longrunning::ListOperationsResponse> ListOperations(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::ListOperationsRequest const& request) = 0;

  virtual StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::GetOperationRequest const& request) = 0;

  virtual Status DeleteOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::DeleteOperationRequest const& request) = 0;

  virtual Status CancelOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::CancelOperationRequest const& request) = 0;

  virtual future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::GetOperationRequest const& request) = 0;

  virtual future<Status> AsyncCancelOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::CancelOperationRequest const& request) = 0;
};

class DefaultWorkflowTemplateServiceStub : public WorkflowTemplateServiceStub {
 public:
  DefaultWorkflowTemplateServiceStub(
      std::unique_ptr<
          google::cloud::dataproc::v1::WorkflowTemplateService::StubInterface>
          grpc_stub,
      std::unique_ptr<google::iam::v1::IAMPolicy::StubInterface> iampolicy_stub,
      std::unique_ptr<google::longrunning::Operations::StubInterface>
          operations_stub)
      : grpc_stub_(std::move(grpc_stub)),
        iampolicy_stub_(std::move(iampolicy_stub)),
        operations_stub_(std::move(operations_stub)) {}

  StatusOr<google::cloud::dataproc::v1::WorkflowTemplate>
  CreateWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::CreateWorkflowTemplateRequest const& request)
      override;

  StatusOr<google::cloud::dataproc::v1::WorkflowTemplate> GetWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::GetWorkflowTemplateRequest const& request)
      override;

  future<StatusOr<google::longrunning::Operation>>
  AsyncInstantiateWorkflowTemplate(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::dataproc::v1::InstantiateWorkflowTemplateRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> InstantiateWorkflowTemplate(
      grpc::ClientContext& context, Options options,
      google::cloud::dataproc::v1::InstantiateWorkflowTemplateRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>>
  AsyncInstantiateInlineWorkflowTemplate(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::dataproc::v1::
          InstantiateInlineWorkflowTemplateRequest const& request) override;

  StatusOr<google::longrunning::Operation> InstantiateInlineWorkflowTemplate(
      grpc::ClientContext& context, Options options,
      google::cloud::dataproc::v1::
          InstantiateInlineWorkflowTemplateRequest const& request) override;

  StatusOr<google::cloud::dataproc::v1::WorkflowTemplate>
  UpdateWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::UpdateWorkflowTemplateRequest const& request)
      override;

  StatusOr<google::cloud::dataproc::v1::ListWorkflowTemplatesResponse>
  ListWorkflowTemplates(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::ListWorkflowTemplatesRequest const& request)
      override;

  Status DeleteWorkflowTemplate(
      grpc::ClientContext& context, Options const& options,
      google::cloud::dataproc::v1::DeleteWorkflowTemplateRequest const& request)
      override;

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::SetIamPolicyRequest const& request) override;

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::GetIamPolicyRequest const& request) override;

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext& context, Options const& options,
      google::iam::v1::TestIamPermissionsRequest const& request) override;

  StatusOr<google::longrunning::ListOperationsResponse> ListOperations(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::ListOperationsRequest const& request) override;

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::GetOperationRequest const& request) override;

  Status DeleteOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::DeleteOperationRequest const& request) override;

  Status CancelOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::CancelOperationRequest const& request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::GetOperationRequest const& request) override;

  future<Status> AsyncCancelOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::CancelOperationRequest const& request) override;

 private:
  std::unique_ptr<
      google::cloud::dataproc::v1::WorkflowTemplateService::StubInterface>
      grpc_stub_;
  std::unique_ptr<google::iam::v1::IAMPolicy::StubInterface> iampolicy_stub_;
  std::unique_ptr<google::longrunning::Operations::StubInterface>
      operations_stub_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace dataproc_v1_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DATAPROC_V1_INTERNAL_WORKFLOW_TEMPLATE_STUB_H
