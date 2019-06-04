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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/protobuf/struct.pb.h>
#include <google/spanner/v1/type.pb.h>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * The Value class represents a type-safe, nullable Spanner value.
 *
 * It is conceptually similar to a `std::any` except the only allowed types are
 * those supported by Spanner, and a "null" value (similar to a `std::any`
 * without a value) still has an associated type that can be queried with
 * `Value::is<T>()`. The supported types are shown in the following table along
 * with how they map to the Spanner types
 * (https://cloud.google.com/spanner/docs/data-types):
 *
 *     Spanner Type | C++ Type
 *     -----------------------
 *     BOOL         | bool
 *     INT64        | std::int64_t
 *     FLOAT64      | double
 *     STRING       | std::string
 *
 * This is a regular C++ value type with support for copy, move, equality, etc,
 * but there is no default constructor because there is no default type.
 * Callers may create instances by passing any of the supported values (shown
 * in the table above) to the constructor. "Null" values are created using the
 * `Value::MakeNull<T>()` factory function.
 *
 * Example with a non-null value:
 *
 *     std::string msg = "hello";
 *     spanner::Value v(msg);
 *     assert(v.is<std::string>());
 *     assert(!v.is_null());
 *     StatusOr<std::string> copy = v.get<std::string>();
 *     if (copy) {
 *       std::cout << *copy;  // prints "hello"
 *     }
 *
 * Example with a null:
 *
 *     spanner::Value v = spanner::Value::MakeNull<std::int64_t>();
 *     assert(v.is<std::int64_t>());
 *     assert(v.is_null());
 *     StatusOr<std::int64_t> i = v.get<std::int64_t>();
 *     if (!i) {
 *       std::cerr << "error: " << i.status();
 *     }
 */
class Value {
 public:
  /// Factory to construct a "null" Value of the specified type `T`.
  template <typename T>
  static Value MakeNull() {
    return Value(Null<T>{});
  }

  Value() = delete;

  /// Copy and move.
  Value(Value const&) = default;
  Value(Value&&) = default;
  Value& operator=(Value const&) = default;
  Value& operator=(Value&&) = default;

  /// Constructs a non-null instance with the specified value and type.
  explicit Value(bool v);
  explicit Value(std::int64_t v);
  explicit Value(double v);
  explicit Value(std::string v);

  friend bool operator==(Value a, Value b);
  friend bool operator!=(Value a, Value b) { return !(a == b); }

  /// Returns true if there is no contained value.
  bool is_null() const;

  /**
   * Returns true if the contained value is of the specified type `T`.
   *
   * All Value instances have some type, even null values.
   *
   * Example:
   *
   *     spanner::Value v{true};
   *     assert(v.is<bool>());
   *
   *     spanner::Value null_v = spanner::Value::MakeNull<bool>();
   *     assert(v.is<bool>());
   */
  template <typename T>
  bool is() const {
    return type_.code() == TypeCodeMap::GetCode(T{});
  }

  /**
   * Returns the contained value wrapped in a `google::cloud::StatusOr<T>`.
   *
   * If the specified type `T` is wrong or if the contained value is "null",
   * then a non-OK Status will be returned instead of the value.
   *
   * Example:
   *
   *     spanner::Value v{3.14};
   *     StatusOr<double> d = v.get<double>();
   *     if (d) {
   *       std::cout << "d=" << *d;
   *     }
   *
   *     // Now using a "null" std::int64_t
   *     v = spanner::Value::MakeNull<std::int64_t>();
   *     assert(v.is_null());
   *     StatusOr<std::int64_t> i = v.get<std::int64_t>();
   *     if (!i) {
   *       std::cerr << "Could not get integer: " << i.status();
   *     }
   */
  template <typename T>
  StatusOr<T> get() const {
    return GetValue(T{});
  }

  /**
   * Returns the contained value iff it is not null and the the specified `T` is
   * correct.
   *
   * It is the caller's responsibility to ensure that this the specifed type
   * `T` is correct (e.g., with `is<T>()`) and that the value is not "null"
   * (e.g., with `!v.is_null()`). Otherwise, the behavior is undefined.
   *
   * This is mostly useful if writing generic code where casting is needed.
   *
   * Example:
   *
   *     spanner::Value v{true};
   *     bool b = static_cast<bool>(v);  // OK: b == true
   *
   *     spanner::Value null_v = spanner::Value::MakeNull<std::int64_t>();
   *     std::int64_t i = static_cast<std::int64_t>(null_v);  // UB! null
   *     bool bad = static_cast<bool>(null_v);  // UB! wrong type
   */
  template <typename T>
  explicit operator T() const {
    return *get<T>();
  }

  /**
   * Allows Google Test to print internal debugging information when test
   * assertions fail.
   *
   * @warning This is intended for debugging and human consumption only, not
   * machine consumption as the output format may change without notice.
   */
  friend void PrintTo(Value const& v, std::ostream* os);

 private:
  // A private argument type and constructor that's used by the public
  // MakeNull<T>() factory function for constructing "null" Values with the
  // specified type.
  template <typename T>
  struct Null {};
  template <typename T>
  explicit Value(Null<T>) {
    *this = Value(T{});
    value_.set_null_value(google::protobuf::NullValue::NULL_VALUE);
  }

  struct TypeCodeMap {
    using Code = google::spanner::v1::TypeCode;
    // Tag-dispatched function overloads. The argument type is important, the
    // value is ignored.
    static Code GetCode(bool) { return Code::BOOL; }
    static Code GetCode(std::int64_t) { return Code::INT64; }
    static Code GetCode(double) { return Code::FLOAT64; }
    static Code GetCode(std::string) { return Code::STRING; }
  };

  // Tag-dispatched function overloads. The argument type is important, the
  // value is ignored.
  StatusOr<bool> GetValue(bool) const;
  StatusOr<std::int64_t> GetValue(std::int64_t) const;
  StatusOr<double> GetValue(double) const;
  StatusOr<std::string> GetValue(std::string) const;

  google::spanner::v1::Type type_;
  google::protobuf::Value value_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H_
