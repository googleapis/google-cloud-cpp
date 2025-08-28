// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H

#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <google/protobuf/arena.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/struct.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class PartialResultSourceInterface : public spanner::ResultSourceInterface {
 public:
  /**
   * A precommit token is included if the read-write transaction is on
   * a multiplexed session. The precommit token with the highest sequence
   * number from this transaction attempt is added to the Commit request for
   * this transaction by the library.
   */
  virtual absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
  PrecommitToken() const {
    return absl::nullopt;
  }
};

/**
 * This class serves as a bridge between the gRPC `PartialResultSet` streaming
 * reader and the spanner `ResultSet`, and is used to iterate over the rows
 * returned from a read operation.
 */
class PartialResultSetSource : public PartialResultSourceInterface {
 public:
  /// Factory method to create a PartialResultSetSource.
  static StatusOr<std::unique_ptr<PartialResultSourceInterface>> Create(
      std::unique_ptr<PartialResultSetReader> reader);

  ~PartialResultSetSource() override;

  StatusOr<spanner::Row> NextRow() override;

  absl::optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    return metadata_;
  }

  absl::optional<google::spanner::v1::ResultSetStats> Stats() const override {
    return stats_;
  }

  absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
  PrecommitToken() const override {
    return precommit_token_;
  }

 private:
  explicit PartialResultSetSource(
      std::unique_ptr<PartialResultSetReader> reader);

  Status ReadFromStream();

  // Arena for the values_ field.
  google::protobuf::Arena arena_;

  Options options_;
  std::unique_ptr<PartialResultSetReader> reader_;

  // The `PartialResultSet.metadata` we received in the first response, and
  // the column names it contained (which will be shared between rows).
  absl::optional<google::spanner::v1::ResultSetMetadata> metadata_;
  std::shared_ptr<std::vector<std::string>> columns_;

  // The `PartialResultSet.stats` received in the last response, corresponding
  // to the `QueryMode` implied by the particular streaming read/query type.
  absl::optional<google::spanner::v1::ResultSetStats> stats_;

  // Each PartialResultSet proto message can contain a token when using a
  // multiplexed session.
  absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
      precommit_token_ = absl::nullopt;

  // Number of rows returned to the client.
  int rows_returned_ = 0;

  // Number of rows that can be created from `values_` in NextRow(). Note there
  // may be more data in values_ but it's not ready to be returned to the
  // client.
  int usable_rows_ = 0;

  // Values that can be assembled into `Row`s ready to be returned by
  // `NextRow()`.
  absl::optional<google::protobuf::RepeatedPtrField<google::protobuf::Value>> values_;

  // When engaged, the token we can use to resume the stream immediately after
  // any data in (or previously in) `rows_`. When disengaged, we have already
  // delivered data that would be replayed, so resumption is disabled until we
  // see a new token.
  absl::optional<std::string> resume_token_ = "";

  // Should the space used by `values_` get larger than this limit, we will
  // move complete rows into `rows_` and disable resumption until we see a
  // new token. During this time, an error in the stream will be returned by
  // `NextRow()`. No individual row in a result set can exceed 100 MiB, so we
  // set the default limit to twice that.
  std::size_t values_space_limit_ = 2 * 100 * (std::size_t{1} << 20);

  // `*values_.rbegin()` exists, but it is incomplete. The rest of the value
  // will be sent in subsequent `PartialResultSet` messages.
  bool values_back_incomplete_ = false;

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
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
