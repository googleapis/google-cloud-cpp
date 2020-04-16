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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_READ_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_READ_RESULT_H

#include "google/cloud/bigquery/row.h"
#include "google/cloud/bigquery/row_set.h"
#include "google/cloud/bigquery/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
namespace internal {

class ReadResultSource {
 public:
  virtual ~ReadResultSource() = default;
  virtual StatusOr<optional<Row>> NextRow() = 0;
  virtual std::size_t CurrentOffset() = 0;
  virtual double FractionConsumed() = 0;
};

}  // namespace internal

// Represents the result of a read operation.
//
// Note that at most one pass can be made over the data returned from a
// `ReadResult`.
class ReadResult {
 public:
  ReadResult() = default;
  explicit ReadResult(std::unique_ptr<internal::ReadResultSource> source)
      : source_(std::move(source)) {}

  // Returns a `RowSet` which can be used to iterate through the rows that are
  // presented by this object.
  RowSet<Row> Rows() {
    return RowSet<Row>([this]() mutable { return source_->NextRow(); });
  }

  // Returns a zero-based index of the last row returned by the `Rows()`
  // iterator. If no rows have been read yet, returns -1.
  int CurrentOffset() { return source_->CurrentOffset(); }

  // Returns a value between 0 and 1, inclusive, that indicates the estimated
  // progress in the result set based on the number of rows the server has
  // processed.
  //
  // Note that if this ReadResult was created through
  // `bigquery::Client::ParallelRead()` or if a row filter was provided, then
  // the returned value will not necessarily equal to the current offset divided
  // by the number of rows in the `ReadResult`:
  //
  //   * In the case of a parallel read, data are assigned to
  //     `bigquery::ReadStream`s lazily by the server. The server does not know
  //     the total number of rows that will be assigned to the stream ahead of
  //     time, so it uses a denominator that is guaranteed to never exceed the
  //     maximum number of rows that are allowed to be assigned.
  //
  //   * In the presence of a row filter, the denominator is not known until all
  //     rows are read because some rows may be excluded. As such, the server
  //     uses an estimate for the number of pre-filtering rows.
  //
  double FractionConsumed() { return source_->FractionConsumed(); }

 private:
  std::unique_ptr<internal::ReadResultSource> source_;
};

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_READ_RESULT_H
