// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H

#include "google/cloud/bigtable/internal/partial_result_set_reader.h"
#include "google/cloud/bigtable/results.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/struct.pb.h>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class PartialResultSourceInterface : public bigtable::ResultSourceInterface {};

/**
 * Class used to iterate over the rows returned from a read operations.
 */
class PartialResultSetSource : public PartialResultSourceInterface {
 public:
  /// Factory method to create a PartialResultSetSource.
  static StatusOr<std::unique_ptr<PartialResultSourceInterface>> Create(
      absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
      std::unique_ptr<PartialResultSetReader> reader);

  ~PartialResultSetSource() override;

  StatusOr<bigtable::QueryRow> NextRow() override;

  absl::optional<google::bigtable::v2::ResultSetMetadata> Metadata() override {
    return metadata_;
  }

 private:
  explicit PartialResultSetSource(
      absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
      std::unique_ptr<PartialResultSetReader> reader);

  Status ReadFromStream();
  Status ProcessDataFromStream(google::bigtable::v2::PartialResultSet& result);
  std::string read_buffer_;

  // Arena for the values_ field.
  google::protobuf::Arena arena_;

  Options options_;
  std::unique_ptr<PartialResultSetReader> reader_;

  // The ResultSetMetadata is received in the first response. It is received
  // from ExecuteQueryResponse
  absl::optional<google::bigtable::v2::ResultSetMetadata> metadata_;

  std::shared_ptr<std::vector<std::string>> columns_;
  std::deque<bigtable::QueryRow> rows_;
  std::vector<bigtable::QueryRow> uncommitted_rows_;

  // Values that can be assembled into `QueryRow`s ready to be returned by
  // `NextRow()`. This is a pointer to an arena-allocated RepeatedPtrField.
  absl::optional<google::protobuf::RepeatedPtrField<google::protobuf::Value>*>
      values_;

  // When engaged, the token we can use to resume the stream immediately after
  // any data in (or previously in) `rows_`. When disengaged, we have already
  // delivered data that would be replayed, so resumption is disabled until we
  // see a new token.
  absl::optional<std::string> resume_token_ = "";

  // The default value is set to 2*256 MiB. A single row in Bigtable can't
  // exceed 256 MiB so setting the limit to twice that size to provide a safe
  // upper bound for the buffer.
  std::size_t values_space_limit_ =
      2 * 256 * (std::size_t{1} << 20);  // 512 MiB

  // The state of our PartialResultSetReader.
  enum : char {
    // `Read()` has yet to return nullopt.
    kReading,
    // `Read()` has returned nullopt, but we are yet to call `Finish()`.
    kEndOfStream,
    // `Finish()` has been called, which means `NextRow()` has returned
    // either an empty row or an error status.
    kFinished,
  } state_ = kReading;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
