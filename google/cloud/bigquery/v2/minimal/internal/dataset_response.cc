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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_response.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

bool valid_dataset(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag") && j.contains("id") &&
          j.contains("datasetReference"));
}

bool valid_list_format_dataset(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("id") &&
          j.contains("datasetReference"));
}

bool valid_datasets_list(nlohmann::json const& j) {
  return (j.contains("kind") && j.contains("etag"));
}

StatusOr<nlohmann::json> parse_json(std::string const& payload) {
  // Build the dataset response object from Http response.
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return internal::InternalError("Error parsing Json from response payload",
                                   GCP_ERROR_INFO());
  }

  return json;
}

}  // namespace

StatusOr<GetDatasetResponse> GetDatasetResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_dataset(*json)) {
    return internal::InternalError("Not a valid Json Dataset object",
                                   GCP_ERROR_INFO());
  }

  GetDatasetResponse result;
  result.dataset = json->get<Dataset>();
  result.http_response = http_response;

  return result;
}

StatusOr<ListDatasetsResponse> ListDatasetsResponse::BuildFromHttpResponse(
    BigQueryHttpResponse const& http_response) {
  auto json = parse_json(http_response.payload);
  if (!json) return std::move(json).status();

  if (!valid_datasets_list(*json)) {
    return internal::InternalError("Not a valid Json DatasetList object",
                                   GCP_ERROR_INFO());
  }

  ListDatasetsResponse result;
  result.http_response = http_response;

  result.kind = json->value("kind", "");
  result.etag = json->value("etag", "");
  result.next_page_token = json->value("nextPageToken", "");

  if (json->contains("datasets")) {
    for (auto const& kv : json->at("datasets").items()) {
      auto const& json_list_format_dataset_obj = kv.value();
      if (!valid_list_format_dataset(json_list_format_dataset_obj)) {
        return internal::InternalError(
            "Not a valid Json ListFormatDataset object", GCP_ERROR_INFO());
      }
      auto const& list_format_dataset =
          json_list_format_dataset_obj.get<ListFormatDataset>();
      result.datasets.push_back(list_format_dataset);
    }
  }

  return result;
}

std::string GetDatasetResponse::DebugString(absl::string_view name,
                                            TracingOptions const& options,
                                            int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("dataset", dataset)
      .SubMessage("http_response", http_response)
      .Build();
}

std::string ListDatasetsResponse::DebugString(absl::string_view name,
                                              TracingOptions const& options,
                                              int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .StringField("next_page_token", next_page_token)
      .Field("datasets", datasets)
      .SubMessage("http_response", http_response)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
