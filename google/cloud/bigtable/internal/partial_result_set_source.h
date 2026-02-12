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

#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/internal/partial_result_set_reader.h"
#include "google/cloud/bigtable/result_source_interface.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include "google/bigtable/v2/bigtable.pb.h"
#include <google/protobuf/arena.h>
#include <google/protobuf/repeated_field.h>
#include "google/protobuf/struct.pb.h"
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class PartialResultSetSource : public bigtable::ResultSourceInterface {
 public:
  /// Factory method to create a PartialResultSetSource.
  static StatusOr<std::unique_ptr<bigtable::ResultSourceInterface>> Create(
      absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
      std::shared_ptr<OperationContext> operation_context,
      std::unique_ptr<PartialResultSetReader> reader);

  ~PartialResultSetSource() override;

  StatusOr<bigtable::QueryRow> NextRow() override;

  absl::optional<google::bigtable::v2::ResultSetMetadata> Metadata() override {
    return metadata_;
  }

 private:
  explicit PartialResultSetSource(
      absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
      std::shared_ptr<OperationContext> operation_context,
      std::unique_ptr<PartialResultSetReader> reader);

  Status ReadFromStream();
  Status ProcessDataFromStream(google::bigtable::v2::PartialResultSet& result);
  Status BufferProtoRows();
  std::string read_buffer_;

  // Arena for the values_ field.
  google::protobuf::Arena arena_;

  Options options_;
  std::unique_ptr<PartialResultSetReader> reader_;
  std::shared_ptr<OperationContext> operation_context_;
  // The ResultSetMetadata is received in the first response. It is received
  // from ExecuteQueryResponse
  absl::optional<google::bigtable::v2::ResultSetMetadata> metadata_;

  std::shared_ptr<std::vector<std::string>> columns_;
  std::deque<bigtable::QueryRow> rows_;
  std::vector<bigtable::QueryRow> buffered_rows_;
  google::bigtable::v2::ProtoRows proto_rows_;

  // An opaque token sent by the server to allow query resumption and signal
  // that the buffered values constructed from received `partial_rows` can be
  // yielded to the caller. Clients can provide this token in a subsequent
  // request to resume the result stream from the current point.
  //
  // When `resume_token` is non-empty, the buffered values received from
  // `partial_rows` since the last non-empty `resume_token` can be yielded to
  // the callers, provided that the client keeps the value of `resume_token` and
  // uses it on subsequent retries.
  //
  // A `resume_token` may be sent without information in `partial_rows` to
  // checkpoint the progress of a sparse query. Any previous `partial_rows` data
  // should still be yielded in this case, and the new `resume_token` should be
  // saved for future retries as normal.
  //
  // A `resume_token` will only be sent on a boundary where there is either no
  // ongoing result batch, or `batch_checksum` is also populated.
  //
  // The server will also send a sentinel `resume_token` when last batch of
  // `partial_rows` is sent. If the client retries the ExecuteQueryRequest with
  // the sentinel `resume_token`, the server will emit it again without any
  // data in `partial_rows`, then return OK.
  absl::optional<std::string> resume_token_ = "";

  Status last_status_;
  // The state of our PartialResultSetReader.
  enum class State {
    // `Read()` has yet to return false.
    kReading,
    // `Read()` has returned false, but we are yet to call `Finish()`.
    kEndOfStream,
    // `Finish()` has been called, which means `NextRow()` has returned
    // either an empty row or an error status.
    kFinished,
  };
  State state_ = State::kReading;
};

/**
 * Helper class that represents a RowStream with non-ok status.
 * When iterated over, it will produce a single StatusOr<>.
 */
class StatusOnlyResultSetSource : public bigtable::ResultSourceInterface {
 public:
  explicit StatusOnlyResultSetSource(Status status)
      : status_(std::move(status)) {}

  StatusOr<bigtable::QueryRow> NextRow() override { return status_; }
  absl::optional<google::bigtable::v2::ResultSetMetadata> Metadata() override {
    return {};
  }

 private:
  Status status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
