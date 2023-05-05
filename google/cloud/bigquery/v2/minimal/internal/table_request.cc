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

#include "google/cloud/bigquery/v2/minimal/internal/table_request.h"
#include "google/cloud/bigquery/v2/minimal/internal/rest_stub_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string GetTableRequest::DebugString(absl::string_view name,
                                         TracingOptions const& options,
                                         int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .StringField("dataset_id", dataset_id_)
      .StringField("table_id", table_id_)
      .Field("selected_fields", selected_fields_)
      .SubMessage("view", view_)
      .Build();
}

std::string ListTablesRequest::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id_)
      .StringField("dataset_id", dataset_id_)
      .Field("max_results", max_results_)
      .StringField("page_token", page_token_)
      .Build();
}

ListTablesRequest::ListTablesRequest(std::string project_id,
                                     std::string dataset_id)
    : project_id_(std::move(project_id)),
      dataset_id_(std::move(dataset_id)),
      max_results_(0) {}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    GetTableRequest const& r) {
  auto const& opts = internal::CurrentOptions();

  rest_internal::RestRequest request;

  std::string endpoint = GetBaseEndpoint(opts);

  std::string path =
      absl::StrCat(endpoint, "/projects/", r.project_id(), "/datasets/",
                   r.dataset_id(), "/tables/", r.table_id());
  request.SetPath(std::move(path));

  // Add query params.
  if (!r.selected_fields().empty()) {
    std::string fields;
    std::size_t index = 0;
    for (auto const& f : r.selected_fields()) {
      if (++index < r.selected_fields().size()) {
        absl::StrAppend(&fields, f, ",");
      } else {
        absl::StrAppend(&fields, f);
      }
    }
    request.AddQueryParameter("selectedFields", fields);
  }

  auto if_not_empty_add = [&](char const* key, auto const& v) {
    if (v.empty()) return;
    request.AddQueryParameter(key, v);
  };
  if_not_empty_add("view", r.view().value);

  return request;
}

StatusOr<rest_internal::RestRequest> BuildRestRequest(
    ListTablesRequest const& r) {
  rest_internal::RestRequest request;
  auto const& opts = internal::CurrentOptions();

  std::string endpoint = GetBaseEndpoint(opts);

  std::string path = absl::StrCat(endpoint, "/projects/", r.project_id(),
                                  "/datasets/", r.dataset_id(), "/tables");
  request.SetPath(std::move(path));

  // Add query params.
  if (r.max_results() > 0) {
    request.AddQueryParameter("maxResults", std::to_string(r.max_results()));
  }

  auto if_not_empty_add = [&](char const* key, auto const& v) {
    if (v.empty()) return;
    request.AddQueryParameter(key, v);
  };
  if_not_empty_add("pageToken", r.page_token());

  return request;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
