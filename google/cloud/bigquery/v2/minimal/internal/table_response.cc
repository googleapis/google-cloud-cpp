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

#include "google/cloud/bigquery/v2/minimal/internal/table_response.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/numbers.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

bool valid_table(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("id") &&
          j.contains("tableReference"));
}

bool valid_list_format_table(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("id") &&
          j.contains("tableReference"));
}

bool valid_tables_list(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("tables"));
}

StatusOr<nlohmann::json> parse_json(std::string const& payload) {
  // Build the table response object from Http response.
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError("Error parsing Json from response payload",
                                   GCP_ERROR_INFO());
  }

  return json;
}

}  // namespace

StatusOr<GetTableResponse> GetTableResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);

  if (!json) return std::move(json).status();

  if (!valid_table(*json)) {
    return internal::InternalError("Not a valid Json Table object",
                                   GCP_ERROR_INFO());
  }

  GetTableResponse result;
  result.table = json->get<Table>();
  result.http_response = http_response;

  return result;
}

StatusOr<ListTablesResponse> ListTablesResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_tables_list(*json)) {
    return internal::InternalError("Not a valid Json TableList object",
                                   GCP_ERROR_INFO());
  }

  ListTablesResponse result;
  result.http_response = http_response;

  result.kind = json->value("kind", "");
  result.etag = json->value("etag", "");
  result.next_page_token = json->value("nextPageToken", "");
  result.total_items = json->value("totalItems", 0);

  for (auto const& kv : json->at("tables").items()) {
    auto const& json_list_format_table_obj = kv.value();
    if (!valid_list_format_table(json_list_format_table_obj)) {
      return internal::InternalError("Not a valid Json ListFormatTable object",
                                     GCP_ERROR_INFO());
    }
    auto const& list_format_table =
        json_list_format_table_obj.get<ListFormatTable>();
    result.tables.push_back(list_format_table);
  }

  return result;
}

std::string GetTableResponse::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("table", table)
      .SubMessage("http_response", http_response)
      .Build();
}

std::string ListTablesResponse::DebugString(absl::string_view name,
                                            TracingOptions const& options,
                                            int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .StringField("next_page_token", next_page_token)
      .Field("total_items", total_items)
      .Field("tables", tables)
      .SubMessage("http_response", http_response)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
