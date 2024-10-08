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
// source: google/cloud/functions/v1/functions.proto

#include "google/cloud/functions/v1/cloud_functions_client.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace functions_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

CloudFunctionsServiceClient::CloudFunctionsServiceClient(
    std::shared_ptr<CloudFunctionsServiceConnection> connection, Options opts)
    : connection_(std::move(connection)),
      options_(
          internal::MergeOptions(std::move(opts), connection_->options())) {}
CloudFunctionsServiceClient::~CloudFunctionsServiceClient() = default;

StreamRange<google::cloud::functions::v1::CloudFunction>
CloudFunctionsServiceClient::ListFunctions(
    google::cloud::functions::v1::ListFunctionsRequest request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ListFunctions(std::move(request));
}

StatusOr<google::cloud::functions::v1::CloudFunction>
CloudFunctionsServiceClient::GetFunction(std::string const& name,
                                         Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::GetFunctionRequest request;
  request.set_name(name);
  return connection_->GetFunction(request);
}

StatusOr<google::cloud::functions::v1::CloudFunction>
CloudFunctionsServiceClient::GetFunction(
    google::cloud::functions::v1::GetFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GetFunction(request);
}

future<StatusOr<google::cloud::functions::v1::CloudFunction>>
CloudFunctionsServiceClient::CreateFunction(
    std::string const& location,
    google::cloud::functions::v1::CloudFunction const& function, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::CreateFunctionRequest request;
  request.set_location(location);
  *request.mutable_function() = function;
  return connection_->CreateFunction(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::CreateFunction(
    NoAwaitTag, std::string const& location,
    google::cloud::functions::v1::CloudFunction const& function, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::CreateFunctionRequest request;
  request.set_location(location);
  *request.mutable_function() = function;
  return connection_->CreateFunction(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::functions::v1::CloudFunction>>
CloudFunctionsServiceClient::CreateFunction(
    google::cloud::functions::v1::CreateFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CreateFunction(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::CreateFunction(
    NoAwaitTag,
    google::cloud::functions::v1::CreateFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CreateFunction(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::functions::v1::CloudFunction>>
CloudFunctionsServiceClient::CreateFunction(
    google::longrunning::Operation const& operation, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CreateFunction(operation);
}

future<StatusOr<google::cloud::functions::v1::CloudFunction>>
CloudFunctionsServiceClient::UpdateFunction(
    google::cloud::functions::v1::CloudFunction const& function, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::UpdateFunctionRequest request;
  *request.mutable_function() = function;
  return connection_->UpdateFunction(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::UpdateFunction(
    NoAwaitTag, google::cloud::functions::v1::CloudFunction const& function,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::UpdateFunctionRequest request;
  *request.mutable_function() = function;
  return connection_->UpdateFunction(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::functions::v1::CloudFunction>>
CloudFunctionsServiceClient::UpdateFunction(
    google::cloud::functions::v1::UpdateFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->UpdateFunction(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::UpdateFunction(
    NoAwaitTag,
    google::cloud::functions::v1::UpdateFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->UpdateFunction(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::functions::v1::CloudFunction>>
CloudFunctionsServiceClient::UpdateFunction(
    google::longrunning::Operation const& operation, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->UpdateFunction(operation);
}

future<StatusOr<google::cloud::functions::v1::OperationMetadataV1>>
CloudFunctionsServiceClient::DeleteFunction(std::string const& name,
                                            Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::DeleteFunctionRequest request;
  request.set_name(name);
  return connection_->DeleteFunction(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::DeleteFunction(NoAwaitTag, std::string const& name,
                                            Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::DeleteFunctionRequest request;
  request.set_name(name);
  return connection_->DeleteFunction(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::functions::v1::OperationMetadataV1>>
CloudFunctionsServiceClient::DeleteFunction(
    google::cloud::functions::v1::DeleteFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->DeleteFunction(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::DeleteFunction(
    NoAwaitTag,
    google::cloud::functions::v1::DeleteFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->DeleteFunction(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::functions::v1::OperationMetadataV1>>
CloudFunctionsServiceClient::DeleteFunction(
    google::longrunning::Operation const& operation, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->DeleteFunction(operation);
}

StatusOr<google::cloud::functions::v1::CallFunctionResponse>
CloudFunctionsServiceClient::CallFunction(std::string const& name,
                                          std::string const& data,
                                          Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::functions::v1::CallFunctionRequest request;
  request.set_name(name);
  request.set_data(data);
  return connection_->CallFunction(request);
}

StatusOr<google::cloud::functions::v1::CallFunctionResponse>
CloudFunctionsServiceClient::CallFunction(
    google::cloud::functions::v1::CallFunctionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CallFunction(request);
}

StatusOr<google::cloud::functions::v1::GenerateUploadUrlResponse>
CloudFunctionsServiceClient::GenerateUploadUrl(
    google::cloud::functions::v1::GenerateUploadUrlRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GenerateUploadUrl(request);
}

StatusOr<google::cloud::functions::v1::GenerateDownloadUrlResponse>
CloudFunctionsServiceClient::GenerateDownloadUrl(
    google::cloud::functions::v1::GenerateDownloadUrlRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GenerateDownloadUrl(request);
}

StatusOr<google::iam::v1::Policy> CloudFunctionsServiceClient::SetIamPolicy(
    google::iam::v1::SetIamPolicyRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->SetIamPolicy(request);
}

StatusOr<google::iam::v1::Policy> CloudFunctionsServiceClient::GetIamPolicy(
    google::iam::v1::GetIamPolicyRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GetIamPolicy(request);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
CloudFunctionsServiceClient::TestIamPermissions(
    google::iam::v1::TestIamPermissionsRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->TestIamPermissions(request);
}

StreamRange<google::cloud::location::Location>
CloudFunctionsServiceClient::ListLocations(
    google::cloud::location::ListLocationsRequest request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ListLocations(std::move(request));
}

StreamRange<google::longrunning::Operation>
CloudFunctionsServiceClient::ListOperations(std::string const& name,
                                            std::string const& filter,
                                            Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::longrunning::ListOperationsRequest request;
  request.set_name(name);
  request.set_filter(filter);
  return connection_->ListOperations(request);
}

StreamRange<google::longrunning::Operation>
CloudFunctionsServiceClient::ListOperations(
    google::longrunning::ListOperationsRequest request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ListOperations(std::move(request));
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::GetOperation(std::string const& name,
                                          Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::longrunning::GetOperationRequest request;
  request.set_name(name);
  return connection_->GetOperation(request);
}

StatusOr<google::longrunning::Operation>
CloudFunctionsServiceClient::GetOperation(
    google::longrunning::GetOperationRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GetOperation(request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace functions_v1
}  // namespace cloud
}  // namespace google
