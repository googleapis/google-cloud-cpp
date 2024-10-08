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
// source: google/cloud/dialogflow/cx/v3/experiment.proto

#include "google/cloud/dialogflow_cx/internal/experiments_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status_or.h"
#include <google/cloud/dialogflow/cx/v3/experiment.grpc.pb.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace dialogflow_cx_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ExperimentsStub::~ExperimentsStub() = default;

StatusOr<google::cloud::dialogflow::cx::v3::ListExperimentsResponse>
DefaultExperimentsStub::ListExperiments(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::ListExperimentsRequest const& request) {
  google::cloud::dialogflow::cx::v3::ListExperimentsResponse response;
  auto status = grpc_stub_->ListExperiments(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::dialogflow::cx::v3::Experiment>
DefaultExperimentsStub::GetExperiment(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::GetExperimentRequest const& request) {
  google::cloud::dialogflow::cx::v3::Experiment response;
  auto status = grpc_stub_->GetExperiment(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::dialogflow::cx::v3::Experiment>
DefaultExperimentsStub::CreateExperiment(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::CreateExperimentRequest const& request) {
  google::cloud::dialogflow::cx::v3::Experiment response;
  auto status = grpc_stub_->CreateExperiment(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::dialogflow::cx::v3::Experiment>
DefaultExperimentsStub::UpdateExperiment(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::UpdateExperimentRequest const& request) {
  google::cloud::dialogflow::cx::v3::Experiment response;
  auto status = grpc_stub_->UpdateExperiment(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

Status DefaultExperimentsStub::DeleteExperiment(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::DeleteExperimentRequest const& request) {
  google::protobuf::Empty response;
  auto status = grpc_stub_->DeleteExperiment(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return google::cloud::Status();
}

StatusOr<google::cloud::dialogflow::cx::v3::Experiment>
DefaultExperimentsStub::StartExperiment(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::StartExperimentRequest const& request) {
  google::cloud::dialogflow::cx::v3::Experiment response;
  auto status = grpc_stub_->StartExperiment(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::dialogflow::cx::v3::Experiment>
DefaultExperimentsStub::StopExperiment(
    grpc::ClientContext& context, Options const&,
    google::cloud::dialogflow::cx::v3::StopExperimentRequest const& request) {
  google::cloud::dialogflow::cx::v3::Experiment response;
  auto status = grpc_stub_->StopExperiment(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::location::ListLocationsResponse>
DefaultExperimentsStub::ListLocations(
    grpc::ClientContext& context, Options const&,
    google::cloud::location::ListLocationsRequest const& request) {
  google::cloud::location::ListLocationsResponse response;
  auto status = locations_stub_->ListLocations(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::cloud::location::Location> DefaultExperimentsStub::GetLocation(
    grpc::ClientContext& context, Options const&,
    google::cloud::location::GetLocationRequest const& request) {
  google::cloud::location::Location response;
  auto status = locations_stub_->GetLocation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::longrunning::ListOperationsResponse>
DefaultExperimentsStub::ListOperations(
    grpc::ClientContext& context, Options const&,
    google::longrunning::ListOperationsRequest const& request) {
  google::longrunning::ListOperationsResponse response;
  auto status = operations_stub_->ListOperations(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

StatusOr<google::longrunning::Operation> DefaultExperimentsStub::GetOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::GetOperationRequest const& request) {
  google::longrunning::Operation response;
  auto status = operations_stub_->GetOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return response;
}

Status DefaultExperimentsStub::CancelOperation(
    grpc::ClientContext& context, Options const&,
    google::longrunning::CancelOperationRequest const& request) {
  google::protobuf::Empty response;
  auto status = operations_stub_->CancelOperation(&context, request, &response);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return google::cloud::Status();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace dialogflow_cx_internal
}  // namespace cloud
}  // namespace google
