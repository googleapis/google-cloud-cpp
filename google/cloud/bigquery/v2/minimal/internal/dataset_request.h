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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_REQUEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_REQUEST_H

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

// Holds request parameters necessary to make the GetDataset call.
class GetDatasetRequest {
 public:
  GetDatasetRequest() = default;
  explicit GetDatasetRequest(std::string project_id, std::string dataset_id)
      : project_id_(std::move(project_id)),
        dataset_id_(std::move(dataset_id)) {}

  std::string const& project_id() const { return project_id_; }
  std::string const& dataset_id() const { return dataset_id_; }

  GetDatasetRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  GetDatasetRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  GetDatasetRequest& set_dataset_id(std::string dataset_id) & {
    dataset_id_ = std::move(dataset_id);
    return *this;
  }
  GetDatasetRequest&& set_dataset_id(std::string dataset_id) && {
    return std::move(set_dataset_id(std::move(dataset_id)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

 private:
  std::string project_id_;
  std::string dataset_id_;
};

class ListDatasetsRequest {
 public:
  ListDatasetsRequest() = default;
  explicit ListDatasetsRequest(std::string project_id);

  std::string const& project_id() const { return project_id_; }
  bool const& all_datasets() const { return all_datasets_; }
  std::int32_t const& max_results() const { return max_results_; }

  std::string const& page_token() const { return page_token_; }
  std::string const& filter() const { return filter_; }

  ListDatasetsRequest& set_project_id(std::string project_id) & {
    project_id_ = std::move(project_id);
    return *this;
  }
  ListDatasetsRequest&& set_project_id(std::string project_id) && {
    return std::move(set_project_id(std::move(project_id)));
  }

  ListDatasetsRequest& set_all_datasets(bool all_datasets) & {
    all_datasets_ = all_datasets;
    return *this;
  }
  ListDatasetsRequest&& set_all_datasets(bool all_datasets) && {
    return std::move(set_all_datasets(all_datasets));
  }

  ListDatasetsRequest& set_max_results(std::int32_t max_results) & {
    max_results_ = max_results;
    return *this;
  }
  ListDatasetsRequest&& set_max_results(std::int32_t max_results) && {
    return std::move(set_max_results(max_results));
  }

  ListDatasetsRequest& set_page_token(std::string page_token) & {
    page_token_ = std::move(page_token);
    return *this;
  }
  ListDatasetsRequest&& set_page_token(std::string page_token) && {
    return std::move(set_page_token(std::move(page_token)));
  }

  ListDatasetsRequest& set_filter(std::string filter) & {
    filter_ = std::move(filter);
    return *this;
  }
  ListDatasetsRequest&& set_filter(std::string filter) && {
    return std::move(set_filter(std::move(filter)));
  }

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  // Members
 private:
  std::string project_id_;
  bool all_datasets_;
  std::int32_t max_results_;
  std::string page_token_;
  std::string filter_;
};

// Builds RestRequest from GetDatasetRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    GetDatasetRequest const& r);
// Builds RestRequest from ListDatasetsRequest.
StatusOr<rest_internal::RestRequest> BuildRestRequest(
    ListDatasetsRequest const& r);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_REQUEST_H
