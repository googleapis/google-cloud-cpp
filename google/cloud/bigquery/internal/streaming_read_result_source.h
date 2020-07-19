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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_INTERNAL_STREAMING_READ_RESULT_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_INTERNAL_STREAMING_READ_RESULT_SOURCE_H

#include "google/cloud/bigquery/internal/stream_reader.h"
#include "google/cloud/bigquery/read_result.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <google/cloud/bigquery/storage/v1beta1/storage.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

class StreamingReadResultSource : public ReadResultSource {
 public:
  explicit StreamingReadResultSource(
      std::unique_ptr<StreamReader<
          google::cloud::bigquery::storage::v1beta1::ReadRowsResponse>>
          reader)
      : reader_(std::move(reader)),
        offset_in_curr_response_(0),
        offset_(0),
        fraction_consumed_(0) {}

  StatusOr<absl::optional<Row>> NextRow() override;
  std::size_t CurrentOffset() override { return offset_; }
  double FractionConsumed() override { return fraction_consumed_; }

 private:
  std::unique_ptr<
      StreamReader<google::cloud::bigquery::storage::v1beta1::ReadRowsResponse>>
      reader_;

  absl::optional<google::cloud::bigquery::storage::v1beta1::ReadRowsResponse>
      curr_;
  std::int64_t offset_in_curr_response_;
  std::size_t offset_;
  double fraction_consumed_;
};

}  // namespace internal
}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_INTERNAL_STREAMING_READ_RESULT_SOURCE_H
