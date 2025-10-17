
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_SQL_STATEMENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_SQL_STATEMENT_H

#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct SqlStatementInternals;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/**
 * Represents parameter of an SQL statement
 *
 * Parameter placeholders are specified by `@<param name>` in the SQL string.
 * Values for parameters are a collection of `std::pair<std::string const,
 * google::cloud:bigtable::Value>`.
 */
class Parameter {
 public:
  explicit Parameter(bool v) : Parameter(PrivateConstructor{}, v) {}
  explicit Parameter(std::int64_t v) : Parameter(PrivateConstructor{}, v) {}
  explicit Parameter(float v) : Parameter(PrivateConstructor{}, v) {}
  explicit Parameter(double v) : Parameter(PrivateConstructor{}, v) {}
  explicit Parameter(std::string v)
      : Parameter(PrivateConstructor{}, std::move(v)) {}
  explicit Parameter(char const* v)
      : Parameter(PrivateConstructor{}, std::move(v)) {}
  explicit Parameter(Bytes const& v)
      : Parameter(PrivateConstructor{}, std::move(v)) {}
  explicit Parameter(Timestamp const& v)
      : Parameter(PrivateConstructor{}, std::move(v)) {}
  explicit Parameter(absl::CivilDay const& v)
      : Parameter(PrivateConstructor{}, std::move(v)) {}

  // Copy and move.
  Parameter(Parameter const&) = default;
  Parameter(Parameter&&) = default;
  Parameter& operator=(Parameter const&) = default;
  Parameter& operator=(Parameter&&) = default;

  google::bigtable::v2::Type const& type() const { return value_.type_; }

  friend bool operator==(Parameter const& a, Parameter const& b);
  friend bool operator!=(Parameter const& a, Parameter const& b) {
    return !(a == b);
  }

  friend std::ostream& operator<<(std::ostream& os, Parameter const& p);

 private:
  struct PrivateConstructor {};

  template <typename T>
  Parameter(PrivateConstructor, T&& val) : value_(std::forward<T>(val)) {}

  Value value_;
};

/**
 * Represents a potentially parameterized SQL statement.
 *
 * @note `SqlStatement` equality comparisons are case-sensitive.
 *
 * Parameter placeholders are specified by `@<param name>` in the SQL string.
 * Values for parameters are a collection of `std::pair<std::string const,
 * google::cloud:bigtable::Value>`.
 */
class SqlStatement {
 public:
  /// Type alias for parameter collection.
  /// The key represents the name of the parameter.
  using ParamType = std::unordered_map<std::string, Parameter>;

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
   * Returns the requested parameter.
   * @param parameter_name name of requested parameter.
   * @return `StatusCode::kNotFound` returned for invalid names.
   */
  google::cloud::StatusOr<Parameter> GetParameter(
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
  friend struct bigtable_internal::SqlStatementInternals;

  std::string statement_;
  ParamType params_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable

// Internal implementation details that callers should not use.
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
using PrepareQueryProto = ::google::bigtable::v2::PrepareQueryRequest;
struct SqlStatementInternals {
  static PrepareQueryProto ToProto(bigtable::SqlStatement s,
                                   bigtable::InstanceResource const& r);
};

inline PrepareQueryProto ToProto(bigtable::SqlStatement s,
                                 bigtable::InstanceResource const& r) {
  return SqlStatementInternals::ToProto(std::move(s), r);
}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_SQL_STATEMENT_H
