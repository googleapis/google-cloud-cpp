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
// source: google/cloud/batch/v1/batch.proto

#include "google/cloud/batch/v1/internal/batch_tracing_stub.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace batch_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

BatchServiceTracingStub::BatchServiceTracingStub(
    std::shared_ptr<BatchServiceStub> child)
    : child_(std::move(child)), propagator_(internal::MakePropagator()) {}

StatusOr<google::cloud::batch::v1::Job> BatchServiceTracingStub::CreateJob(
    grpc::ClientContext& context, Options const& options,
    google::cloud::batch::v1::CreateJobRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "CreateJob");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->CreateJob(context, options, request));
}

StatusOr<google::cloud::batch::v1::Job> BatchServiceTracingStub::GetJob(
    grpc::ClientContext& context, Options const& options,
    google::cloud::batch::v1::GetJobRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "GetJob");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetJob(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
BatchServiceTracingStub::AsyncDeleteJob(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::batch::v1::DeleteJobRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "DeleteJob");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncDeleteJob(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation> BatchServiceTracingStub::DeleteJob(
    grpc::ClientContext& context, Options options,
    google::cloud::batch::v1::DeleteJobRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "DeleteJob");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->DeleteJob(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
BatchServiceTracingStub::AsyncCancelJob(
    google::cloud::CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::cloud::batch::v1::CancelJobRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "CancelJob");
  internal::OTelScope scope(span);
  internal::InjectTraceContext(*context, *propagator_);
  auto f = child_->AsyncCancelJob(cq, context, std::move(options), request);
  return internal::EndSpan(std::move(context), std::move(span), std::move(f));
}

StatusOr<google::longrunning::Operation> BatchServiceTracingStub::CancelJob(
    grpc::ClientContext& context, Options options,
    google::cloud::batch::v1::CancelJobRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "CancelJob");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->CancelJob(context, options, request));
}

StatusOr<google::cloud::batch::v1::ListJobsResponse>
BatchServiceTracingStub::ListJobs(
    grpc::ClientContext& context, Options const& options,
    google::cloud::batch::v1::ListJobsRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "ListJobs");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListJobs(context, options, request));
}

StatusOr<google::cloud::batch::v1::Task> BatchServiceTracingStub::GetTask(
    grpc::ClientContext& context, Options const& options,
    google::cloud::batch::v1::GetTaskRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "GetTask");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetTask(context, options, request));
}

StatusOr<google::cloud::batch::v1::ListTasksResponse>
BatchServiceTracingStub::ListTasks(
    grpc::ClientContext& context, Options const& options,
    google::cloud::batch::v1::ListTasksRequest const& request) {
  auto span =
      internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService", "ListTasks");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListTasks(context, options, request));
}

StatusOr<google::cloud::location::ListLocationsResponse>
BatchServiceTracingStub::ListLocations(
    grpc::ClientContext& context, Options const& options,
    google::cloud::location::ListLocationsRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService",
                                     "ListLocations");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListLocations(context, options, request));
}

StatusOr<google::cloud::location::Location>
BatchServiceTracingStub::GetLocation(
    grpc::ClientContext& context, Options const& options,
    google::cloud::location::GetLocationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService",
                                     "GetLocation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetLocation(context, options, request));
}

StatusOr<google::longrunning::ListOperationsResponse>
BatchServiceTracingStub::ListOperations(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::ListOperationsRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService",
                                     "ListOperations");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->ListOperations(context, options, request));
}

StatusOr<google::longrunning::Operation> BatchServiceTracingStub::GetOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::GetOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService",
                                     "GetOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->GetOperation(context, options, request));
}

Status BatchServiceTracingStub::DeleteOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::DeleteOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService",
                                     "DeleteOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->DeleteOperation(context, options, request));
}

Status BatchServiceTracingStub::CancelOperation(
    grpc::ClientContext& context, Options const& options,
    google::longrunning::CancelOperationRequest const& request) {
  auto span = internal::MakeSpanGrpc("google.cloud.batch.v1.BatchService",
                                     "CancelOperation");
  auto scope = opentelemetry::trace::Scope(span);
  internal::InjectTraceContext(context, *propagator_);
  return internal::EndSpan(context, *span,
                           child_->CancelOperation(context, options, request));
}

future<StatusOr<google::longrunning::Operation>>
BatchServiceTracingStub::AsyncGetOperation(
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

future<Status> BatchServiceTracingStub::AsyncCancelOperation(
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

std::shared_ptr<BatchServiceStub> MakeBatchServiceTracingStub(
    std::shared_ptr<BatchServiceStub> stub) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return std::make_shared<BatchServiceTracingStub>(std::move(stub));
#else
  return stub;
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace batch_v1_internal
}  // namespace cloud
}  // namespace google
