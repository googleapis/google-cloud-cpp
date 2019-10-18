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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_RESULTS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_RESULTS_H_

#include "google/cloud/spanner/row_parser.h"
#include "google/cloud/spanner/timestamp.h"
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
 */
using ExecutionPlan = google::spanner::v1::QueryPlan;

namespace internal {
class ResultSourceInterface {
 public:
  virtual ~ResultSourceInterface() = default;
  // Returns OK Status with no Value to indicate end-of-stream.
  virtual StatusOr<optional<Value>> NextValue() = 0;
  virtual optional<google::spanner::v1::ResultSetMetadata> Metadata() = 0;
  virtual optional<google::spanner::v1::ResultSetStats> Stats() const = 0;
};
}  // namespace internal

/**
 * Represents the result of a read operation using `spanner::Client::Read()`.
 *
 * Note that a `QueryResult` returns the data for the operation, as a
 * single-pass, input range returned by `Rows()`.
 */
class QueryResult {
 public:
  QueryResult() = default;
  explicit QueryResult(std::unique_ptr<internal::ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  QueryResult(QueryResult&&) = default;
  QueryResult& operator=(QueryResult&&) = default;

  /**
   * Returns a `RowParser` which can be used to iterate the returned
   * `std::tuple`s.
   *
   * Since there is a single result stream for each `QueryResult` instance,
   * users should not use multiple `RowParser`s from the same `QueryResult` at
   * the same time. Doing so is not thread safe, and may result in errors or
   * data corruption.
   */
  template <typename RowType>
  RowParser<RowType> Rows() {
    return RowParser<RowType>(
        [this]() mutable { return source_->NextValue(); });
  }

  /**
   * Retrieve the timestamp at which the read occurred.
   *
   * Only available if a read-only transaction was used, and the timestamp
   * was requested by setting `return_read_timestamp` true.
   */
  optional<Timestamp> ReadTimestamp() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};

/**
 * Represents the result of a data modifying operation using
 * `spanner::Client::ExecuteDML()`.
 *
 * This class encapsulates the result of a Cloud Spanner DML operation, i.e.,
 * `INSERT`, `UPDATE`, or `DELETE`.
 *
 * @note `ExecuteDmlResult` returns the number of rows modified, query plan
 * (if requested), and execution statistics (if requested).
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
   * other DML statements provide an exact count.
   */
  std::int64_t RowsModified() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};

class ProfileQueryResult {
 public:
  ProfileQueryResult() = default;
  explicit ProfileQueryResult(
      std::unique_ptr<internal::ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  ProfileQueryResult(ProfileQueryResult&&) = default;
  ProfileQueryResult& operator=(ProfileQueryResult&&) = default;

  /**
   * Returns a `RowParser` which can be used to iterate the returned
   * `std::tuple`s.
   *
   * Since there is a single result stream for each `ProfileQueryResult`
   * instance, users should not use multiple `RowParser`s from the same
   * `ProfileQueryResult` at the same time. Doing so is not thread safe, and may
   * result in errors or data corruption.
   */
  template <typename RowType>
  RowParser<RowType> Rows() {
    return RowParser<RowType>(
        [this]() mutable { return source_->NextValue(); });
  }

  /**
   * Retrieve the timestamp at which the read occurred.
   *
   * Only available if a read-only transaction was used, and the timestamp
   * was requested by setting `return_read_timestamp` true.
   */
  optional<Timestamp> ReadTimestamp() const;

  /**
   * Returns a collection of key value pair statistics for the SQL statement
   * execution.
   *
   * @note Only available when the statement is executed and all results have
   * been read.
   */
  optional<std::unordered_map<std::string, std::string>> ExecutionStats() const;

  /**
   * Returns the plan of execution for the SQL statement.
   */
  optional<spanner::ExecutionPlan> ExecutionPlan() const;

 private:
  std::unique_ptr<internal::ResultSourceInterface> source_;
};

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

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_RESULTS_H_
