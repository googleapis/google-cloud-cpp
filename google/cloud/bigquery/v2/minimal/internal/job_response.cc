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

#include "google/cloud/bigquery/v2/minimal/internal/job_response.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool valid_job(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("id") &&
          j.contains("status") && j.contains("reference") &&
          j.contains("configuration"));
}

bool valid_list_format_job(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("state") && j.contains("id") &&
          j.contains("reference") && j.contains("configuration"));
}

bool valid_jobs_list(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") &&
          j.contains("next_page_token") && j.contains("jobs"));
}

StatusOr<nlohmann::json> parse_json(std::string const& payload) {
  if (payload.empty()) {
    return internal::InternalError("Empty payload in HTTP response",
                                   GCP_ERROR_INFO());
  }
  // Build the job response object from Http response.
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError("Error parsing Json from response payload",
                                   GCP_ERROR_INFO());
  }

  return json;
}

StatusOr<GetJobResponse> GetJobResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_job(*json)) {
    return internal::InternalError("Not a valid Json Job object",
                                   GCP_ERROR_INFO());
  }

  GetJobResponse result;
  result.job = json->get<Job>();
  result.http_response = http_response;

  return result;
}

StatusOr<ListJobsResponse> ListJobsResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_jobs_list(*json)) {
    return internal::InternalError("Not a valid Json JobList object",
                                   GCP_ERROR_INFO());
  }

  ListJobsResponse result;
  result.http_response = http_response;

  result.kind = json->value("kind", "");
  result.etag = json->value("etag", "");
  result.next_page_token = json->value("next_page_token", "");

  for (auto const& kv : json->at("jobs").items()) {
    auto const& json_list_format_job_obj = kv.value();
    if (!valid_list_format_job(json_list_format_job_obj)) {
      return internal::InternalError("Not a valid Json ListFormatJob object",
                                     GCP_ERROR_INFO());
    }
    auto const& list_format_job = json_list_format_job_obj.get<ListFormatJob>();
    result.jobs.push_back(list_format_job);
  }

  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
