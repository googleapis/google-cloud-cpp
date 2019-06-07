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

#include "google/cloud/optional.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/protobuf/struct.pb.h>
#include <google/spanner/v1/type.pb.h>
#include <ostream>
#include <string>
#include <type_traits>

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
 *     ARRAY        | std::vector<T>  // [1]
 *
 * [1] The type `T` may be any of the other supported types, except for
 *     ARRAY/std::vector.
 *
 * Value is a regular C++ value type with support for copy, move, equality,
 * etc, but there is no default constructor because there is no default type.
 * Callers may create instances by passing any of the supported values (shown
 * in the table above) to the constructor. "Null" values are created using the
 * `Value::MakeNull<T>()` factory function or by passing an empty `optional<T>`
 * to the Value constructor..
 *
 * Example with a non-null value:
 *
 *     std::string msg = "hello";
 *     spanner::Value v(msg);
 *     assert(v.is<std::string>());
 *     assert(!v.is_null<std::string>());
 *     StatusOr<std::string> copy = v.get<std::string>();
 *     if (copy) {
 *       std::cout << *copy;  // prints "hello"
 *     }
 *
 * Example with a null:
 *
 *     spanner::Value v = spanner::Value::MakeNull<std::int64_t>();
 *     assert(v.is<std::int64_t>());
 *     assert(v.is_null<std::int64_t>());
 *     StatusOr<std::int64_t> i = v.get<std::int64_t>();
 *     assert(!i.ok());  // Can't get the value becuase v is null
 *     StatusOr<optional<std::int64_t> j = v.get<optional<std::int64_t>>();
 *     assert(j.ok());  // OK because an empty option can represent the null
 */
class Value {
 public:
  /**
   * Factory to construct a "null" Value of the specified type `T`.
   *
   * This is equivalent to passing to the constructor an `optional<T>` without
   * a value, though this factory may result in clearer code at the call site.
   */
  template <typename T>
  static Value MakeNull() {
    return Value(optional<T>{});
  }

  Value() = delete;

  /// Copy and move.
  Value(Value const&) = default;
  Value(Value&&) = default;
  Value& operator=(Value const&) = default;
  Value& operator=(Value&&) = default;

  /// Constructs a non-null instance with the specified type and value.
  explicit Value(bool v);
  explicit Value(std::int64_t v);
  explicit Value(double v);
  explicit Value(std::string v);

  /**
   * Constructs a non-null instance if `opt` has a value, otherwise constructs
   * a null instance.
   */
  template <typename T>
  explicit Value(optional<T> const& opt) {
    if (opt.has_value()) {
      *this = Value(*opt);
    } else {
      type_ = GetType(T{});
      value_.set_null_value(google::protobuf::NullValue::NULL_VALUE);
    }
  }

  /**
   * Constructs an instance from a Spanner ARRAY of the specified type and
   * values.
   *
   * The type `T` may be any valid type shown above, except vectors of vectors
   * are not allowed.
   */
  template <typename T>  // TODO(#59): add an enabler to disallow T==vector
  explicit Value(std::vector<T> const& v) {
    type_ = GetType(v);
    for (auto const& e : v) {
      *value_.mutable_list_value()->add_values() = std::move(Value(e).value_);
    }
  }

  friend bool operator==(Value a, Value b);
  friend bool operator!=(Value a, Value b) { return !(a == b); }

  /**
   * Returns true if the contained value is of the specified type `T`.
   *
   * All Value instances have some type, even null values. `T` may be an
   * `optional<U>`, in which case it will return the same value as `is<U>()`.
   *
   * Example:
   *
   *     spanner::Value v{true};
   *     assert(v.is<bool>());  // Same as the following
   *     assert(v.is<optional<bool>>());
   *
   *     spanner::Value null_v = spanner::Value::MakeNull<bool>();
   *     assert(v.is<bool>());  // Same as the following
   *     assert(v.is<optional<bool>>());
   */
  template <typename T>
  bool is() const {
    return ProtoEqual(type_, GetType(T{}));
  }

  /**
   * Returns true if is<T>() and the contained value is "null".
   *
   * `T` may be an `optional<U>`, in which case it will return the same value
   * as `is_null<U>()`.
   *
   * Example:
   *
   *     spanner::Value v{true};
   *     assert(!v.is_null<bool>());  // Same as the following
   *     assert(!v.is_null<optional<bool>>());
   *
   *     spanner::Value null_v = spanner::Value::MakeNull<bool>();
   *     assert(v.is_null<bool>());  // Same as the following
   *     assert(v.is_null<optional<bool>>());
   *     assert(!v.is_null<std::int64_t>());  // Same as the following
   *     assert(!v.is_null<optional<std::int64_t>>());
   */
  template <typename T>
  bool is_null() const {
    return is<T>() && value_.kind_case() == google::protobuf::Value::kNullValue;
  }

  /**
   * Returns the contained value wrapped in a `google::cloud::StatusOr<T>`.
   *
   * Requires `is<T>()` is true, otherwise a non-OK Status will be returned. If
   * `is_null<T>()` is true, a non-OK Status is returned, unless the specified
   * type `T` is an `optional<U>`, in which case a valueless optional is
   * returned to indicate the nullness.
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
   *     assert(v.is_null<std::int64_t>());
   *     StatusOr<std::int64_t> i = v.get<std::int64_t>();
   *     if (!i) {
   *       std::cerr << "Could not get integer: " << i.status();
   *     }
   *
   *     StatusOr<optional<std::int64_t>> j = v.get<optional<std::int64_t>();
   *     assert(j.ok());  // Since we know the types match in this example
   *     assert(!v->has_value());  // Since we know v was null in this example
   */
  template <typename T>
  StatusOr<T> get() const {
    if (!is<T>()) return Status(StatusCode::kInvalidArgument, "wrong type");
    if (value_.kind_case() == google::protobuf::Value::kNullValue) {
      if (is_optional<T>::value) return T{};
      return Status(StatusCode::kInvalidArgument, "null value");
    }
    return GetValue(T{}, value_);
  }

  /**
   * Returns the contained value iff is<T>() and !is_null<T>().
   *
   * It is the caller's responsibility to ensure that the specified type
   * `T` is correct (e.g., with `is<T>()`) and that the value is not "null"
   * (e.g., with `!v.is_null<T>()`). Otherwise, the behavior is undefined.
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
  // Metafunction that returns true if `T` is an optional<U>
  template <typename T>
  struct is_optional : std::false_type {};
  template <typename T>
  struct is_optional<optional<T>> : std::true_type {};

  // Tag-dispatch overloads to convert a C++ type to a `Type` protobuf. The
  // argument type is the tag, the argument value is ignored.
  static google::spanner::v1::Type GetType(bool);
  static google::spanner::v1::Type GetType(std::int64_t);
  static google::spanner::v1::Type GetType(double);
  static google::spanner::v1::Type GetType(std::string const&);
  template <typename T>
  static google::spanner::v1::Type GetType(optional<T> const&) {
    return GetType(T{});
  }
  template <typename T>
  static google::spanner::v1::Type GetType(std::vector<T> const&) {
    google::spanner::v1::Type t;
    t.set_code(google::spanner::v1::TypeCode::ARRAY);
    *t.mutable_array_element_type() = GetType(T{});  // Recursive call
    return t;
  }

  // Tag-dispatch overloads to extract a C++ value from a `Value` protobuf. The
  // first argument type is the tag, the first argument value is ignored.
  static bool GetValue(bool, google::protobuf::Value const&);
  static std::int64_t GetValue(std::int64_t, google::protobuf::Value const&);
  static double GetValue(double, google::protobuf::Value const&);
  static std::string GetValue(std::string const&,
                              google::protobuf::Value const&);
  template <typename T>
  static optional<T> GetValue(optional<T> const&,
                              google::protobuf::Value const& pv) {
    return GetValue(T{}, pv);
  }
  template <typename T>
  static std::vector<T> GetValue(std::vector<T> const&,
                                 google::protobuf::Value const& pv) {
    std::vector<T> v;
    for (auto const& e : pv.list_value().values()) {
      v.push_back(GetValue(T{}, e));
    }
    return v;
  }

  // Helper to compare protos for equality.
  static bool ProtoEqual(google::protobuf::Message const& m1,
                         google::protobuf::Message const& m2);

  google::spanner::v1::Type type_;
  google::protobuf::Value value_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H_
