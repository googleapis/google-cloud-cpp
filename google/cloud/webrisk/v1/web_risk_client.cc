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
// source: google/cloud/webrisk/v1/webrisk.proto

#include "google/cloud/webrisk/v1/web_risk_client.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace webrisk_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

WebRiskServiceClient::WebRiskServiceClient(
    std::shared_ptr<WebRiskServiceConnection> connection, Options opts)
    : connection_(std::move(connection)),
      options_(
          internal::MergeOptions(std::move(opts), connection_->options())) {}
WebRiskServiceClient::~WebRiskServiceClient() = default;

StatusOr<google::cloud::webrisk::v1::ComputeThreatListDiffResponse>
WebRiskServiceClient::ComputeThreatListDiff(
    google::cloud::webrisk::v1::ThreatType threat_type,
    std::string const& version_token,
    google::cloud::webrisk::v1::ComputeThreatListDiffRequest::Constraints const&
        constraints,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::webrisk::v1::ComputeThreatListDiffRequest request;
  request.set_threat_type(threat_type);
  request.set_version_token(version_token);
  *request.mutable_constraints() = constraints;
  return connection_->ComputeThreatListDiff(request);
}

StatusOr<google::cloud::webrisk::v1::ComputeThreatListDiffResponse>
WebRiskServiceClient::ComputeThreatListDiff(
    google::cloud::webrisk::v1::ComputeThreatListDiffRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ComputeThreatListDiff(request);
}

StatusOr<google::cloud::webrisk::v1::SearchUrisResponse>
WebRiskServiceClient::SearchUris(
    std::string const& uri,
    std::vector<google::cloud::webrisk::v1::ThreatType> const& threat_types,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::webrisk::v1::SearchUrisRequest request;
  request.set_uri(uri);
  *request.mutable_threat_types() = {threat_types.begin(), threat_types.end()};
  return connection_->SearchUris(request);
}

StatusOr<google::cloud::webrisk::v1::SearchUrisResponse>
WebRiskServiceClient::SearchUris(
    google::cloud::webrisk::v1::SearchUrisRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->SearchUris(request);
}

StatusOr<google::cloud::webrisk::v1::SearchHashesResponse>
WebRiskServiceClient::SearchHashes(
    std::string const& hash_prefix,
    std::vector<google::cloud::webrisk::v1::ThreatType> const& threat_types,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::webrisk::v1::SearchHashesRequest request;
  request.set_hash_prefix(hash_prefix);
  *request.mutable_threat_types() = {threat_types.begin(), threat_types.end()};
  return connection_->SearchHashes(request);
}

StatusOr<google::cloud::webrisk::v1::SearchHashesResponse>
WebRiskServiceClient::SearchHashes(
    google::cloud::webrisk::v1::SearchHashesRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->SearchHashes(request);
}

StatusOr<google::cloud::webrisk::v1::Submission>
WebRiskServiceClient::CreateSubmission(
    std::string const& parent,
    google::cloud::webrisk::v1::Submission const& submission, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::webrisk::v1::CreateSubmissionRequest request;
  request.set_parent(parent);
  *request.mutable_submission() = submission;
  return connection_->CreateSubmission(request);
}

StatusOr<google::cloud::webrisk::v1::Submission>
WebRiskServiceClient::CreateSubmission(
    google::cloud::webrisk::v1::CreateSubmissionRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CreateSubmission(request);
}

future<StatusOr<google::cloud::webrisk::v1::Submission>>
WebRiskServiceClient::SubmitUri(
    std::string const& parent,
    google::cloud::webrisk::v1::Submission const& submission, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::webrisk::v1::SubmitUriRequest request;
  request.set_parent(parent);
  *request.mutable_submission() = submission;
  return connection_->SubmitUri(request);
}

StatusOr<google::longrunning::Operation> WebRiskServiceClient::SubmitUri(
    NoAwaitTag, std::string const& parent,
    google::cloud::webrisk::v1::Submission const& submission, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::cloud::webrisk::v1::SubmitUriRequest request;
  request.set_parent(parent);
  *request.mutable_submission() = submission;
  return connection_->SubmitUri(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::webrisk::v1::Submission>>
WebRiskServiceClient::SubmitUri(
    google::cloud::webrisk::v1::SubmitUriRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->SubmitUri(request);
}

StatusOr<google::longrunning::Operation> WebRiskServiceClient::SubmitUri(
    NoAwaitTag, google::cloud::webrisk::v1::SubmitUriRequest const& request,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->SubmitUri(NoAwaitTag{}, request);
}

future<StatusOr<google::cloud::webrisk::v1::Submission>>
WebRiskServiceClient::SubmitUri(google::longrunning::Operation const& operation,
                                Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->SubmitUri(operation);
}

StreamRange<google::longrunning::Operation>
WebRiskServiceClient::ListOperations(std::string const& name,
                                     std::string const& filter, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::longrunning::ListOperationsRequest request;
  request.set_name(name);
  request.set_filter(filter);
  return connection_->ListOperations(request);
}

StreamRange<google::longrunning::Operation>
WebRiskServiceClient::ListOperations(
    google::longrunning::ListOperationsRequest request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ListOperations(std::move(request));
}

StatusOr<google::longrunning::Operation> WebRiskServiceClient::GetOperation(
    std::string const& name, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::longrunning::GetOperationRequest request;
  request.set_name(name);
  return connection_->GetOperation(request);
}

StatusOr<google::longrunning::Operation> WebRiskServiceClient::GetOperation(
    google::longrunning::GetOperationRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GetOperation(request);
}

Status WebRiskServiceClient::DeleteOperation(std::string const& name,
                                             Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::longrunning::DeleteOperationRequest request;
  request.set_name(name);
  return connection_->DeleteOperation(request);
}

Status WebRiskServiceClient::DeleteOperation(
    google::longrunning::DeleteOperationRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->DeleteOperation(request);
}

Status WebRiskServiceClient::CancelOperation(std::string const& name,
                                             Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  google::longrunning::CancelOperationRequest request;
  request.set_name(name);
  return connection_->CancelOperation(request);
}

Status WebRiskServiceClient::CancelOperation(
    google::longrunning::CancelOperationRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CancelOperation(request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace webrisk_v1
}  // namespace cloud
}  // namespace google
