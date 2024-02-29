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

#include "google/cloud/bigquery/v2/minimal/internal/project_response.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/numbers.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

bool valid_project(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("id") &&
          j.contains("projectReference"));
}

bool valid_projects_list(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("totalItems"));
}

StatusOr<nlohmann::json> parse_json(std::string const& payload) {
  // Build the project response object from Http response.
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError("Error parsing Json from response payload",
                                   GCP_ERROR_INFO());
  }

  return json;
}

}  // namespace

StatusOr<ListProjectsResponse> ListProjectsResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_projects_list(*json)) {
    return internal::InternalError("Not a valid Json ProjectList object",
                                   GCP_ERROR_INFO());
  }

  ListProjectsResponse result;
  result.http_response = http_response;

  result.kind = json->value("kind", "");
  result.etag = json->value("etag", "");
  result.next_page_token = json->value("nextPageToken", "");
  result.total_items = json->value("totalItems", 0);

  if (result.total_items == 0) return result;

  for (auto const& kv : json->at("projects").items()) {
    auto const& json_list_format_project_obj = kv.value();
    if (!valid_project(json_list_format_project_obj)) {
      return internal::InternalError("Not a valid Json Project object",
                                     GCP_ERROR_INFO());
    }
    auto const& project = json_list_format_project_obj.get<Project>();
    result.projects.push_back(project);
  }

  return result;
}

std::string ListProjectsResponse::DebugString(absl::string_view name,
                                              TracingOptions const& options,
                                              int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .StringField("next_page_token", next_page_token)
      .Field("total_items", total_items)
      .Field("projects", projects)
      .SubMessage("http_response", http_response)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
