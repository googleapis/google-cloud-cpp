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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SQL_STATEMENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SQL_STATEMENT_H

#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/spanner.pb.h>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct SqlStatementInternals;
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * Represents a potentially parameterized SQL statement.
 *
 * Details on case sensitivity for SQL statements and string values can be
 * found here: [Case
 * Sensitivity](https://cloud.google.com/spanner/docs/lexical#case-sensitivity)
 *
 * @note `SqlStatement` equality comparisons are case-sensitive.
 *
 * Parameter placeholders are specified by `@<param name>` in the SQL string.
 * Values for parameters are a collection of `std::pair<std::string const,
 * google::cloud:spanner::Value>`.
 * @par Example
 * @snippet samples.cc spanner-sql-statement-params
 */
class SqlStatement {
 public:
  /// Type alias for parameter collection.
  using ParamType = std::unordered_map<std::string, Value>;

  SqlStatement() = default;
  /// Constructs a SqlStatement without parameters.
  explicit SqlStatement(std::string statement)
      : statement_(std::move(statement)) {}
  /// Constructs a SqlStatement with specified parameters.
  SqlStatement(std::string statement, ParamType params)
      : statement_(std::move(statement)), params_(std::move(params)) {}

  /// Copy and move.
  SqlStatement(SqlStatement const&) = default;
  SqlStatement(SqlStatement&&) = default;
  SqlStatement& operator=(SqlStatement const&) = default;
  SqlStatement& operator=(SqlStatement&&) = default;

  /**
   * Returns the SQL statement.
   * No parameter substitution is performed in the statement string.
   */
  std::string const& sql() const { return statement_; }

  /**
   * Returns the collection of parameters.
   * @return If no parameters were specified, the container will be empty.
   */
  ParamType const& params() const { return params_; }

  /**
   * Returns the names of all the parameters.
   */
  std::vector<std::string> ParameterNames() const;

  /**
   * Returns the value of the requested parameter.
   * @param parameter_name name of requested parameter.
   * @return `StatusCode::kNotFound` returned for invalid names.
   */
  google::cloud::StatusOr<Value> GetParameter(
      std::string const& parameter_name) const;

  friend bool operator==(SqlStatement const& a, SqlStatement const& b) {
    return a.statement_ == b.statement_ && a.params_ == b.params_;
  }
  friend bool operator!=(SqlStatement const& a, SqlStatement const& b) {
    return !(a == b);
  }

  /**
   * Outputs a string representation of the given @p stmt to the given @p os.
   *
   * @warning This is intended for debugging and human consumption only, not
   *     machine consumption, as the output format may change without notice.
   */
  friend std::ostream& operator<<(std::ostream& os, SqlStatement const& stmt);

 private:
  friend struct spanner_internal::SPANNER_CLIENT_NS::SqlStatementInternals;

  std::string statement_;
  ParamType params_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

// Internal implementation details that callers should not use.
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
// Use this proto type because it conveniently wraps all three attributes
// required to represent a SQL statement.
using SqlStatementProto =
    ::google::spanner::v1::ExecuteBatchDmlRequest::Statement;

struct SqlStatementInternals {
  static SqlStatementProto ToProto(spanner::SqlStatement s);
};

inline SqlStatementProto ToProto(spanner::SqlStatement s) {
  return SqlStatementInternals::ToProto(std::move(s));
}
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SQL_STATEMENT_H
