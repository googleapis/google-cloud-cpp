// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/internal/streaming_read_result_source.h"
#include "google/cloud/bigquery/read_result.h"
#include "google/cloud/bigquery/row.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

namespace {
namespace bigquerystorage_proto = ::google::cloud::bigquery::storage::v1beta1;
}  // namespace

StatusOr<absl::optional<Row>> StreamingReadResultSource::NextRow() {
  if (!curr_ || offset_in_curr_response_ == curr_->row_count()) {
    // Either no response has ever been read from the server or the previous
    // call to this function consumed the last row in the last response.
    auto next = reader_->NextValue();
    if (!next.ok()) {
      return next.status();
    }
    if (!next.value()) {
      return absl::optional<Row>();
    }
    curr_ = std::move(next.value());
    offset_in_curr_response_ = 0;
  }

  // TODO(#18): For now, we're just returning dummy Row objects, one per row in
  // the response. Once we get Apache Arrow set up as a dependency, start
  // parsing the actual data.
  ++offset_in_curr_response_;
  ++offset_;

  bigquerystorage_proto::Progress const& progress = curr_->status().progress();
  fraction_consumed_ =
      progress.at_response_start() +
      (progress.at_response_end() - progress.at_response_start()) *
          offset_in_curr_response_ * 1.0 / curr_->row_count();

  return absl::optional<Row>(Row());
}

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google
