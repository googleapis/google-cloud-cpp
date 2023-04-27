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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_CLIENT_H

#include "google/cloud/bigquery/v2/minimal/internal/dataset_connection.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// BigQuery Dataset Client.
///
/// The Dataset client uses the BigQuery Dataset API to read dataset information
/// from BigQuery.
///
class DatasetClient {
 public:
  explicit DatasetClient(std::shared_ptr<DatasetConnection> connection,
                         Options opts = {});
  ~DatasetClient();

  DatasetClient(DatasetClient const&) = default;
  DatasetClient& operator=(DatasetClient const&) = default;
  DatasetClient(DatasetClient&&) = default;
  DatasetClient& operator=(DatasetClient&&) = default;

  friend bool operator==(DatasetClient const& a, DatasetClient const& b) {
    return a.connection_ == b.connection_;
  }
  friend bool operator!=(DatasetClient const& a, DatasetClient const& b) {
    return !(a == b);
  }

  /// Gets the metadata for the given dataset.
  ///
  /// @see https://cloud.google.com/bigquery/docs/managing-datasets for more details on BigQuery datasets.
  StatusOr<Dataset> GetDataset(GetDatasetRequest const& request,
                               Options opts = {});

  /// Lists all datasets for a project.
  ///
  /// @see https://cloud.google.com/bigquery/docs/managing-datasets for more details on BigQuery datasets.
  StreamRange<ListFormatDataset> ListDatasets(
      ListDatasetsRequest const& request, Options opts = {});

 private:
  std::shared_ptr<DatasetConnection> connection_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_DATASET_CLIENT_H
