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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_REQUEST_H

#include "google/cloud/bigquery/v2/minimal/internal/table_view.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Holds request parameters necessary to make the GetTable call.
class GetTableRequest {
 public:
  GetTableRequest() = default;
  explicit GetTableRequest(std::string project_id, std::string dataset_id,
                           std::string table_id)
      : project_id_(std::move(project_id)),
        dataset_id_(std::move(dataset_id)),
        table_id_(std::move(table_id)) {}

  std::string const& project_id() const { return project_id_; }
  std::string const& dataset_id() const { return dataset_id_; }
  std::string const& table_id() const { return table_id_; }
  std::vector<std::string> const& selected_fields() const {
    return selected_fields_;
  }
  TableMetadataView const& view() const { return view_; }

  GetTableRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  GetTableRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  GetTableRequest& set_dataset_id(std::string dataset_id) & {
    dataset_id_ = std::move(dataset_id);
    return *this;
  }
  GetTableRequest&& set_dataset_id(std::string dataset_id) && {
    return std::move(set_dataset_id(std::move(dataset_id)));
  }

  GetTableRequest& set_table_id(std::string table_id) & {
    table_id_ = std::move(table_id);
    return *this;
  }
  GetTableRequest&& set_table_id(std::string table_id) && {
    return std::move(set_table_id(std::move(table_id)));
  }

  GetTableRequest& set_selected_fields(
      std::vector<std::string> selected_fields) & {
    selected_fields_ = std::move(selected_fields);
    return *this;
  }
  GetTableRequest&& set_selected_fields(
      std::vector<std::string> selected_fields) && {
    return std::move(set_selected_fields(std::move(selected_fields)));
  }

  GetTableRequest& set_view(TableMetadataView view) & {
    view_ = std::move(view);
    return *this;
  }
  GetTableRequest&& set_table_id(TableMetadataView view) && {
    return std::move(set_view(std::move(view)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  std::string dataset_id_;
  std::string table_id_;

  std::vector<std::string> selected_fields_;
  TableMetadataView view_;
};

class ListTablesRequest {
 public:
  ListTablesRequest() = default;
  explicit ListTablesRequest(std::string project_id, std::string dataset_id);

  std::string const& project_id() const { return project_id_; }
  std::string const& dataset_id() const { return dataset_id_; }
  std::int32_t const& max_results() const { return max_results_; }
  std::string const& page_token() const { return page_token_; }

  ListTablesRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  ListTablesRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  ListTablesRequest& set_dataset_id(std::string dataset_id) & {
    dataset_id_ = std::move(dataset_id);
    return *this;
  }
  ListTablesRequest&& set_dataset_id(std::string dataset_id) && {
    return std::move(set_dataset_id(std::move(dataset_id)));
  }

  ListTablesRequest& set_max_results(std::int32_t max_results) & {
    max_results_ = max_results;
    return *this;
  }
  ListTablesRequest&& set_max_results(std::int32_t max_results) && {
    return std::move(set_max_results(max_results));
  }

  ListTablesRequest& set_page_token(std::string page_token) & {
    page_token_ = std::move(page_token);
    return *this;
  }
  ListTablesRequest&& set_page_token(std::string page_token) && {
    return std::move(set_page_token(std::move(page_token)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  // Members
 private:
  std::string project_id_;
  std::string dataset_id_;
  std::int32_t max_results_;
  std::string page_token_;
};

// Builds RestRequest from GetTableRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(GetTableRequest const& r);
// Builds RestRequest from ListTablesRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    ListTablesRequest const& r);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_REQUEST_H
