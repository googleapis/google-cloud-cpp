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

#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

JobClient::JobClient(std::shared_ptr<BigQueryJobConnection> connection,
                     Options opts)
    : connection_(std::move(connection)),
      options_(
          internal::MergeOptions(std::move(opts), connection_->options())) {}
JobClient::~JobClient() = default;

StatusOr<Job> JobClient::GetJob(GetJobRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->GetJob(request);
}

StreamRange<ListFormatJob> JobClient::ListJobs(ListJobsRequest const& request,
                                               Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->ListJobs(request);
}

StatusOr<Job> JobClient::InsertJob(InsertJobRequest const& request,
                                   Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->InsertJob(request);
}

StatusOr<Job> JobClient::CancelJob(CancelJobRequest const& request,
                                   Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->CancelJob(request);
}

StatusOr<PostQueryResults> JobClient::Query(PostQueryRequest const& request,
                                            Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->Query(request);
}

StatusOr<GetQueryResults> JobClient::QueryResults(
    GetQueryResultsRequest const& request, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), options_));
  return connection_->QueryResults(request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
