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
// source: google/cloud/dialogflow/v2/agent.proto

#include "google/cloud/dialogflow_es/internal/agents_tracing_stub.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace dialogflow_es_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

AgentsTracingStub::AgentsTracingStub(std::shared_ptr<AgentsStub> child)
    : child_(std::move(child)), propagator_(internal::MakePropagator()) {}

StatusOr<google::cloud::dialogflow::v2::Agent> AgentsTracingStub::GetAgent(
    grpc::ClientContext& context, Options const& options,
    google::cloud::dialogflow::v2::GetAgentRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents", "GetAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetAgent(context, options, request));
}

StatusOr<google::cloud::dialogflow::v2::Agent> AgentsTracingStub::SetAgent(
    grpc::ClientContext& context, Options const& options,
    google::cloud::dialogflow::v2::SetAgentRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents", "SetAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->SetAgent(context, options, request));
}

Status AgentsTracingStub::DeleteAgent(
    grpc::ClientContext& context, Options const& options,
    google::cloud::dialogflow::v2::DeleteAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "DeleteAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->DeleteAgent(context, options, request));
}

StatusOr<google::cloud::dialogflow::v2::SearchAgentsResponse>
AgentsTracingStub::SearchAgents(
    grpc::ClientContext& context, Options const& options,
    google::cloud::dialogflow::v2::SearchAgentsRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "SearchAgents");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->SearchAgents(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AgentsTracingStub::AsyncTrainAgent(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::dialogflow::v2::TrainAgentRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents", "TrainAgent");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncTrainAgent(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation> AgentsTracingStub::TrainAgent(
    grpc::ClientContext& context, Options options,
    google::cloud::dialogflow::v2::TrainAgentRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents", "TrainAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->TrainAgent(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AgentsTracingStub::AsyncExportAgent(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::dialogflow::v2::ExportAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "ExportAgent");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncExportAgent(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation> AgentsTracingStub::ExportAgent(
    grpc::ClientContext& context, Options options,
    google::cloud::dialogflow::v2::ExportAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "ExportAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ExportAgent(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AgentsTracingStub::AsyncImportAgent(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::dialogflow::v2::ImportAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "ImportAgent");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncImportAgent(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation> AgentsTracingStub::ImportAgent(
    grpc::ClientContext& context, Options options,
    google::cloud::dialogflow::v2::ImportAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "ImportAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ImportAgent(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AgentsTracingStub::AsyncRestoreAgent(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::dialogflow::v2::RestoreAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "RestoreAgent");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncRestoreAgent(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation> AgentsTracingStub::RestoreAgent(
    grpc::ClientContext& context, Options options,
    google::cloud::dialogflow::v2::RestoreAgentRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "RestoreAgent");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->RestoreAgent(context, options, request));
}

StatusOr<google::cloud::dialogflow::v2::ValidationResult>
AgentsTracingStub::GetValidationResult(
    grpc::ClientContext& context, Options const& options,
    google::cloud::dialogflow::v2::GetValidationResultRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "GetValidationResult");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(
      context, *span, child_->GetValidationResult(context, options, request));
}

StatusOr<google::cloud::location::ListLocationsResponse>
AgentsTracingStub::ListLocations(
    grpc::ClientContext& context, Options const& options,
    google::cloud::location::ListLocationsRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "ListLocations");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListLocations(context, options, request));
}

StatusOr<google::cloud::location::Location> AgentsTracingStub::GetLocation(
    grpc::ClientContext& context, Options const& options,
    google::cloud::location::GetLocationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "GetLocation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetLocation(context, options, request));
}

StatusOr<google::longrunning::ListOperationsResponse>
AgentsTracingStub::ListOperations(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::ListOperationsRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "ListOperations");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListOperations(context, options, request));
}

StatusOr<google::longrunning::Operation> AgentsTracingStub::GetOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::GetOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "GetOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetOperation(context, options, request));
}

Status AgentsTracingStub::CancelOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::CancelOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.dialogflow.v2.Agents",
                                     "CancelOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->CancelOperation(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
AgentsTracingStub::AsyncGetOperation(
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

future<Status> AgentsTracingStub::AsyncCancelOperation(
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

std::shared_ptr<AgentsStub> MakeAgentsTracingStub(
    std::shared_ptr<AgentsStub> stub) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return std::make_shared<AgentsTracingStub>(std::move(stub));
#else
  return stub;
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace dialogflow_es_internal
}  // namespace cloud
}  // namespace google
