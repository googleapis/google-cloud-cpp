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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RESULTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RESULTS_H

#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include <google/spanner/v1/spanner.pb.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Contains a hierarchical representation of the operations the database server
 * performs in order to execute a particular SQL statement.
 * [Query Plan
 * proto](https://github.com/googleapis/googleapis/blob/master/google/spanner/v1/query_plan.proto)
 *
 * @par Example:
 * @snippet samples.cc analyze-query
 */
using ExecutionPlan = ::google::spanner::v1::QueryPlan;

namespace internal {
class ResultSourceInterface {
 public:
  virtual ~ResultSourceInterface() = default;
  // Returns OK Status with an empty Row to indicate end-of-stream.
  virtual StatusOr<Row> NextRow() = 0;
  virtual optional<google::spanner::v1::ResultSetMetadata> Metadata() = 0;
  virtual optional<google::spanner::v1::ResultSetStats> Stats() const = 0;
};
}  // namespace internal

/**
 * Represents the stream of `Rows` returned from `spanner::Client::Read()` or
 * `spanner::Client::ExecuteQuery()`.
 *
 * A `RowStream` object is a range defined by the [Input
 * Iterators][input-iterator] returned from its `begin()` and `end()` members.
 * Callers may directly iterate a `RowStream` instance, which will return a
 * sequence of `StatusOr<Row>` objects.
 *
 * For convenience, callers may wrap a `RowStream` instance in a
 * `StreamOf<std::tuple<...>>` object, which will automatically parse each
 * `Row` into a `std::tuple` with the specified types.
 *
 * [input-iterator]: https://en.cppreference.com/w/cpp/named_req/InputIterator
 */
class RowStream {
 public:
  RowStream() = default;
  explicit RowStream(std::unique_ptr<internal::ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  RowStream(RowStream&&) = default;
  RowStream& operator=(RowStream&&) = default;

  /// Returns a `RowStreamIterator` defining the beginning of this range.
  RowStreamIterator begin() {
    return RowStreamIterator([this]() mutable { return source_->NextRow(); });
  }

  /// Returns a `RowStreamIterator` defining the end of this range.
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  RowStreamIterator end() { return {}; }

  /**
   * Retrieves the timestamp at which the read occurred.
   *
   * @note Only available if a read-only transaction was used.
   */
  optional<Timestamp> ReadTimestamp() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};

/**
 * Represents the result of a data modifying operation using
 * `spanner::Client::ExecuteDml()`.
 *
 * This class encapsulates the result of a Cloud Spanner DML operation, i.e.,
 * `INSERT`, `UPDATE`, or `DELETE`.
 *
 * @note `ExecuteDmlResult` returns the number of rows modified.
 *
 * @par Example:
 * @snippet samples.cc execute-dml
 */
class DmlResult {
 public:
  DmlResult() = default;
  explicit DmlResult(std::unique_ptr<internal::ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  DmlResult(DmlResult&&) = default;
  DmlResult& operator=(DmlResult&&) = default;

  /**
   * Returns the number of rows modified by the DML statement.
   *
   * @note Partitioned DML only provides a lower bound of the rows modified, all
   *     other DML statements provide an exact count.
   */
  std::int64_t RowsModified() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};

/**
 * Represents the stream of `Rows` and profile stats returned from
 * `spanner::Client::ProfileQuery()`.
 *
 * A `RowStream` object is a range defined by the [Input
 * Iterators][input-iterator] returned from its `begin()` and `end()` members.
 * Callers may directly iterate a `RowStream` instance, which will return a
 * sequence of `StatusOr<Row>` objects.
 *
 * For convenience, callers may wrap a `RowStream` instance in a
 * `StreamOf<std::tuple<...>>` object, which will automatically parse each
 * `Row` into a `std::tuple` with the specified types.
 *
 * [input-iterator]: https://en.cppreference.com/w/cpp/named_req/InputIterator
 *
 * @par Example:
 * @snippet samples.cc profile-query
 */
class ProfileQueryResult {
 public:
  ProfileQueryResult() = default;
  explicit ProfileQueryResult(
      std::unique_ptr<internal::ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  ProfileQueryResult(ProfileQueryResult&&) = default;
  ProfileQueryResult& operator=(ProfileQueryResult&&) = default;

  /// Returns a `RowStreamIterator` defining the beginning of this result set.
  RowStreamIterator begin() {
    return RowStreamIterator([this]() mutable { return source_->NextRow(); });
  }

  /// Returns a `RowStreamIterator` defining the end of this result set.
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  RowStreamIterator end() { return {}; }

  /**
   * Retrieves the timestamp at which the read occurred.
   *
   * @note Only available if a read-only transaction was used.
   */
  optional<Timestamp> ReadTimestamp() const;

  /**
   * Returns a collection of key value pair statistics for the SQL statement
   * execution.
   *
   * @note Only available when the statement is executed and all results have
   *     been read.
   */
  optional<std::unordered_map<std::string, std::string>> ExecutionStats() const;

  /**
   * Returns the plan of execution for the SQL statement.
   */
  optional<spanner::ExecutionPlan> ExecutionPlan() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};

/**
 * Represents the result and profile stats of a data modifying operation using
 * `spanner::Client::ProfileDml()`.
 *
 * This class encapsulates the result of a Cloud Spanner DML operation, i.e.,
 * `INSERT`, `UPDATE`, or `DELETE`.
 *
 * @note `ProfileDmlResult` returns the number of rows modified, execution
 *     statistics, and query plan.
 *
 * @par Example:
 * @snippet samples.cc profile-dml
 */
class ProfileDmlResult {
 public:
  ProfileDmlResult() = default;
  explicit ProfileDmlResult(
      std::unique_ptr<internal::ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  ProfileDmlResult(ProfileDmlResult&&) = default;
  ProfileDmlResult& operator=(ProfileDmlResult&&) = default;

  /**
   * Returns the number of rows modified by the DML statement.
   *
   * @note Partitioned DML only provides a lower bound of the rows modified, all
   * other DML statements provide an exact count.
   */
  std::int64_t RowsModified() const;

  /**
   * Returns a collection of key value pair statistics for the SQL statement
   * execution.
   *
   * @note Only available when the SQL statement is executed.
   */
  optional<std::unordered_map<std::string, std::string>> ExecutionStats() const;

  /**
   * Returns the plan of execution for the SQL statement.
   */
  optional<spanner::ExecutionPlan> ExecutionPlan() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RESULTS_H
