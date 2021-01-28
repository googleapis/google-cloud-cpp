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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H

#include "google/cloud/spanner/bytes.h"
#include "google/cloud/spanner/date.h"
#include "google/cloud/spanner/internal/tuple_utils.h"
#include "google/cloud/spanner/numeric.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "absl/time/civil_time.h"
#include "absl/types/optional.h"
#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/spanner/v1/type.pb.h>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct ValueInternals;
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * The Value class represents a type-safe, nullable Spanner value.
 *
 * It is conceptually similar to a `std::any` except the only allowed types are
 * those supported by Spanner, and a "null" value (similar to a `std::any`
 * without a value) still has an associated type. The supported types are shown
 * in the following table along with how they map to the Spanner types
 * (https://cloud.google.com/spanner/docs/data-types):
 *
 * Spanner Type | C++ Type `T`
 * ------------ | ------------
 * BOOL         | `bool`
 * INT64        | `std::int64_t`
 * FLOAT64      | `double`
 * STRING       | `std::string`
 * BYTES        | `google::cloud::spanner::Bytes`
 * NUMERIC      | `google::cloud::spanner::Numeric`
 * TIMESTAMP    | `google::cloud::spanner::Timestamp`
 * DATE         | `absl::CivilDay`
 * ARRAY        | `std::vector<T>`  // [1]
 * STRUCT       | `std::tuple<Ts...>`
 *
 * [1] The type `T` may be any of the other supported types, except for
 *     ARRAY/`std::vector`.
 *
 * Value is a regular C++ value type with support for copy, move, equality,
 * etc. A default-constructed Value represents an empty value with no type.
 *
 * @note There is also a C++ type of `CommitTimestamp` that corresponds to a
 *     Cloud Spanner TIMESTAMP object for setting the commit timestamp on a
 *     column with the `allow_commit_timestamp` set to `true` in the schema.
 *
 * @see https://cloud.google.com/spanner/docs/commit-timestamp
 *
 * Callers may create instances by passing any of the supported values
 * (shown in the table above) to the constructor. "Null" values are created
 * using the `MakeNullValue<T>()` factory function or by passing an empty
 * `absl::optional<T>` to the Value constructor..
 *
 * @par Example with a non-null value
 *
 * @code
 * std::string msg = "hello";
 * spanner::Value v(msg);
 * StatusOr<std::string> copy = v.get<std::string>();
 * if (copy) {
 *   std::cout << *copy;  // prints "hello"
 * }
 * @endcode
 *
 * @par Example with a null
 *
 * @code
 * spanner::Value v = spanner::MakeNullValue<std::int64_t>();
 * StatusOr<std::int64_t> i = v.get<std::int64_t>();
 * assert(!i.ok());  // Can't get the value because v is null
 * StatusOr < absl::optional<std::int64_t> j =
 *     v.get<absl::optional<std::int64_t>>();
 * assert(j.ok());  // OK because an empty option can represent the null
 * assert(!j->has_value());  // v held no value.
 * @endcode
 *
 * @par Nullness
 *
 * All of the supported types (above) are "nullable". A null is created in one
 * of two ways:
 *
 * 1. Passing an `absl::optional<T>()` with no value to `Value`'s constructor.
 * 2. Using the `MakeNullValue<T>()` helper function (defined below).
 *
 * Nulls can be retrieved from a `Value::get<T>` by specifying the type `T`
 * as an `absl::optional<U>`. The returned optional will either be empty
 * (indicating null) or it will contain the actual value. See the documentation
 * for `Value::get<T>` below for more details.
 *
 * @par Spanner Arrays (i.e., `std::vector<T>`)
 *
 * Spanner arrays are represented in C++ as a `std::vector<T>`, where the type
 * `T` may be any of the other allowed Spanner types, such as `bool`,
 * `std::int64_t`, etc. The only exception is that arrays may not directly
 * contain another array; to achieve a similar result you could create an array
 * of a 1-element struct holding an array. The following examples show usage of
 * arrays.
 *
 * @code
 * std::vector<std::int64_t> vec = {1, 2, 3, 4, 5};
 * spanner::Value v(vec);
 * auto copy = *v.get<std::vector<std::int64_t>>();
 * assert(vec == copy);
 * @endcode
 *
 * @par Spanner Structs (i.e., `std::tuple<Ts...>`)
 *
 * Spanner structs are represented in C++ as instances of `std::tuple` holding
 * zero or more of the allowed Spanner types, such as `bool`, `std::int64_t`,
 * `std::vector`, and even other `std::tuple` objects. Each tuple element
 * corresponds to a single field in a Spanner STRUCT.
 *
 * Spanner STRUCT fields may optionally contain a string indicating the field's
 * name. Fields names may be empty, unique, or repeated. A named field may be
 * specified as a tuple element of type `std::pair<std::string, T>`, where the
 * pair's `.first` member indicates the field's name, and the `.second` member
 * is any valid Spanner type `T`.
 *
 * @code
 * using Struct = std::tuple<bool, std::pair<std::string, std::int64_t>>;
 * Struct s  = {true, {"Foo", 42}};
 * spanner::Value v(s);
 * assert(s == *v.get<Struct>());
 * @endcode
 *
 * @note While a STRUCT's (optional) field names are not part of its C++ type,
 *   they are part of its Spanner STRUCT type. Array's (i.e., `std::vector`)
 *   must contain a single element type, therefore it is an error to construct
 *   a `std::vector` of `std::tuple` objects with differently named fields.
 */
class Value {
 public:
  /**
   * Constructs a `Value` that holds nothing.
   *
   * All calls to `get<T>()` will return an error.
   */
  Value() = default;

  // Copy and move.
  Value(Value const&) = default;
  Value(Value&&) = default;
  Value& operator=(Value const&) = default;
  Value& operator=(Value&&) = default;

  /// Constructs an instance with the specified type and value.
  explicit Value(bool v) : Value(PrivateConstructor{}, v) {}
  /// @copydoc Value(bool)
  explicit Value(std::int64_t v) : Value(PrivateConstructor{}, v) {}
  /// @copydoc Value(bool)
  explicit Value(double v) : Value(PrivateConstructor{}, v) {}
  /// @copydoc Value(bool)
  explicit Value(std::string v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(Bytes v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(Numeric v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(Timestamp v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(CommitTimestamp v)
      : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(absl::CivilDay v)
      : Value(PrivateConstructor{}, std::move(v)) {}

  /**
   * Constructs an instance from common C++ literal types that closely, though
   * not exactly, match supported Spanner types.
   *
   * An integer literal in C++ is of type `int`, which is not exactly an
   * allowed Spanner type. This will be allowed but it will be implicitly up
   * converted to a `std::int64_t`. Similarly, a C++ string literal will be
   * implicitly converted to a `std::string`. For example:
   *
   * @code
   * spanner::Value v1(42);
   * assert(42 == *v1.get<std::int64_t>());
   *
   * spanner::Value v2("hello");
   * assert("hello" == *v2.get<std::string>());
   * @endcode
   */
  explicit Value(int v) : Value(PrivateConstructor{}, v) {}
  /// @copydoc Value(int)
  explicit Value(char const* v) : Value(PrivateConstructor{}, v) {}

  /**
   * Constructs a non-null instance if `opt` has a value, otherwise constructs
   * a null instance with the specified type `T`.
   */
  template <typename T>
  explicit Value(absl::optional<T> opt)
      : Value(PrivateConstructor{}, std::move(opt)) {}

  /**
   * Constructs an instance from a Spanner ARRAY of the specified type and
   * values.
   *
   * The type `T` may be any valid type shown above, except vectors of vectors
   * are not allowed.
   *
   * @warning If `T` is a `std::tuple` with field names (i.e., at least one of
   *   its element types is a `std::pair<std::string, T>`) then, all of the
   *   vector's elements must have exactly the same field names. Any mismatch
   *   in in field names results in undefined behavior.
   */
  template <typename T>
  explicit Value(std::vector<T> v) : Value(PrivateConstructor{}, std::move(v)) {
    static_assert(!IsVector<typename std::decay<T>::type>::value,
                  "vector of vector not allowed. See value.h documentation.");
  }

  /**
   * Constructs an instance from a Spanner STRUCT with a type and values
   * matching the given `std::tuple`.
   *
   * Any STRUCT field may optionally have a name, which is specified as
   * `std::pair<std::string, T>`.
   */
  template <typename... Ts>
  explicit Value(std::tuple<Ts...> tup)
      : Value(PrivateConstructor{}, std::move(tup)) {}

  friend bool operator==(Value const& a, Value const& b);
  friend bool operator!=(Value const& a, Value const& b) { return !(a == b); }

  /**
   * Returns the contained value wrapped in a `google::cloud::StatusOr<T>`.
   *
   * Returns a non-OK status IFF:
   *
   * * The contained value is "null", and `T` is not an `absl::optional`.
   * * There is an error converting the contained value to `T`.
   *
   * @par Example
   *
   * @code
   * spanner::Value v{3.14};
   * StatusOr<double> d = v.get<double>();
   * if (d) {
   *   std::cout << "d=" << *d;
   * }
   *
   * // Now using a "null" std::int64_t
   * v = spanner::MakeNullValue<std::int64_t>();
   * StatusOr<std::int64_t> i = v.get<std::int64_t>();
   * if (!i) {
   *   std::cerr << "Could not get integer: " << i.status();
   * }
   * StatusOr<absl::optional<std::int64_t>> j =
   *     v.get<absl::optional<std::int64_t>>();
   * assert(j.ok());  // Since we know the types match in this example
   * assert(!v->has_value());  // Since we know v was null in this example
   * @endcode
   */
  template <typename T>
  StatusOr<T> get() const& {
    if (!TypeProtoIs(T{}, type_))
      return Status(StatusCode::kUnknown, "wrong type");
    if (value_.kind_case() == google::protobuf::Value::kNullValue) {
      if (IsOptional<T>::value) return T{};
      return Status(StatusCode::kUnknown, "null value");
    }
    return GetValue(T{}, value_, type_);
  }

  /// @copydoc get()
  template <typename T>
  StatusOr<T> get() && {
    if (!TypeProtoIs(T{}, type_))
      return Status(StatusCode::kUnknown, "wrong type");
    if (value_.kind_case() == google::protobuf::Value::kNullValue) {
      if (IsOptional<T>::value) return T{};
      return Status(StatusCode::kUnknown, "null value");
    }
    auto tag = T{};  // Works around an odd msvc issue
    return GetValue(std::move(tag), std::move(value_), type_);
  }

  /**
   * Outputs string representation of a given Value to the provided stream.
   *
   * @warning This is intended for debugging and human consumption only, not
   *     machine consumption, as the output format may change without notice.
   *
   * @par Example:
   * @code
   *   spanner::Value v{42};
   *   std::cout << v << "\n";
   * @endcode
   */
  friend std::ostream& operator<<(std::ostream& os, Value const& v);

  /**
   * Prints the same output as `operator<<`.
   *
   * @warning DO NOT CALL. This function will be removed in a future release.
   *     Use `operator<<` instead.
   */
  friend void PrintTo(Value const& v, std::ostream* os) { *os << v; }

 private:
  // Metafunction that returns true if `T` is an `absl::optional<U>`
  template <typename T>
  struct IsOptional : std::false_type {};
  template <typename T>
  struct IsOptional<absl::optional<T>> : std::true_type {};

  // Metafunction that returns true if `T` is a std::vector<U>
  template <typename T>
  struct IsVector : std::false_type {};
  template <typename... Ts>
  struct IsVector<std::vector<Ts...>> : std::true_type {};

  // Tag-dispatch overloads to check if a C++ type matches the type specified
  // by the given `Type` proto.
  static bool TypeProtoIs(bool, google::spanner::v1::Type const&);
  static bool TypeProtoIs(std::int64_t, google::spanner::v1::Type const&);
  static bool TypeProtoIs(double, google::spanner::v1::Type const&);
  static bool TypeProtoIs(Timestamp, google::spanner::v1::Type const&);
  static bool TypeProtoIs(CommitTimestamp, google::spanner::v1::Type const&);
  static bool TypeProtoIs(absl::CivilDay, google::spanner::v1::Type const&);
  static bool TypeProtoIs(std::string const&, google::spanner::v1::Type const&);
  static bool TypeProtoIs(Bytes const&, google::spanner::v1::Type const&);
  static bool TypeProtoIs(Numeric const&, google::spanner::v1::Type const&);
  template <typename T>
  static bool TypeProtoIs(absl::optional<T>,
                          google::spanner::v1::Type const& type) {
    return TypeProtoIs(T{}, type);
  }
  template <typename T>
  static bool TypeProtoIs(std::vector<T> const&,
                          google::spanner::v1::Type const& type) {
    return type.code() == google::spanner::v1::TypeCode::ARRAY &&
           TypeProtoIs(T{}, type.array_element_type());
  }
  template <typename... Ts>
  static bool TypeProtoIs(std::tuple<Ts...> const& tup,
                          google::spanner::v1::Type const& type) {
    bool ok = type.code() == google::spanner::v1::TypeCode::STRUCT;
    ok = ok && type.struct_type().fields().size() == sizeof...(Ts);
    spanner_internal::ForEach(tup, IsStructTypeProto{ok, 0},
                              type.struct_type());
    return ok;
  }

  // A functor to be used with internal::ForEach to check if a StructType proto
  // matches the types in a std::tuple.
  struct IsStructTypeProto {
    bool& ok;
    int field;
    template <typename T>
    void operator()(T const&, google::spanner::v1::StructType const& type) {
      ok = ok && TypeProtoIs(T{}, type.fields(field).type());
      ++field;
    }
    template <typename T>
    void operator()(std::pair<std::string, T> const&,
                    google::spanner::v1::StructType const& type) {
      operator()(T{}, type);
    }
  };

  // Tag-dispatch overloads to convert a C++ type to a `Type` protobuf. The
  // argument type is the tag, the argument value is ignored.
  static google::spanner::v1::Type MakeTypeProto(bool);
  static google::spanner::v1::Type MakeTypeProto(std::int64_t);
  static google::spanner::v1::Type MakeTypeProto(double);
  static google::spanner::v1::Type MakeTypeProto(std::string const&);
  static google::spanner::v1::Type MakeTypeProto(Bytes const&);
  static google::spanner::v1::Type MakeTypeProto(Numeric const&);
  static google::spanner::v1::Type MakeTypeProto(Timestamp);
  static google::spanner::v1::Type MakeTypeProto(CommitTimestamp);
  static google::spanner::v1::Type MakeTypeProto(absl::CivilDay);
  static google::spanner::v1::Type MakeTypeProto(int);
  static google::spanner::v1::Type MakeTypeProto(char const*);
  template <typename T>
  static google::spanner::v1::Type MakeTypeProto(absl::optional<T> const&) {
    return MakeTypeProto(T{});
  }
  template <typename T>
  static google::spanner::v1::Type MakeTypeProto(std::vector<T> const& v) {
    google::spanner::v1::Type t;
    t.set_code(google::spanner::v1::TypeCode::ARRAY);
    *t.mutable_array_element_type() = MakeTypeProto(v.empty() ? T{} : v[0]);
    // Checks that vector elements have exactly the same proto Type, which
    // includes field names. This is documented UB.
    for (auto&& e : v) {
      if (!google::protobuf::util::MessageDifferencer::Equals(
              MakeTypeProto(e), t.array_element_type())) {
        google::cloud::internal::ThrowInvalidArgument("Mismatched types");
      }
    }
    return t;
  }
  template <typename... Ts>
  static google::spanner::v1::Type MakeTypeProto(std::tuple<Ts...> const& tup) {
    google::spanner::v1::Type t;
    t.set_code(google::spanner::v1::TypeCode::STRUCT);
    spanner_internal::ForEach(tup, AddStructTypes{}, *t.mutable_struct_type());
    return t;
  }

  // A functor to be used with internal::ForEach to add type protos for all the
  // elements of a tuple.
  struct AddStructTypes {
    template <typename T>
    void operator()(T const& t,
                    google::spanner::v1::StructType& struct_type) const {
      auto* field = struct_type.add_fields();
      *field->mutable_type() = MakeTypeProto(t);
    }
    template <typename S, typename T,
              typename std::enable_if<
                  std::is_convertible<S, std::string>::value, int>::type = 0>
    void operator()(std::pair<S, T> const& p,
                    google::spanner::v1::StructType& struct_type) const {
      auto* field = struct_type.add_fields();
      field->set_name(p.first);
      *field->mutable_type() = MakeTypeProto(p.second);
    }
  };

  // Encodes the argument as a protobuf according to the rules described in
  // https://github.com/googleapis/googleapis/blob/master/google/spanner/v1/type.proto
  static google::protobuf::Value MakeValueProto(bool b);
  static google::protobuf::Value MakeValueProto(std::int64_t i);
  static google::protobuf::Value MakeValueProto(double d);
  static google::protobuf::Value MakeValueProto(std::string s);
  static google::protobuf::Value MakeValueProto(Bytes b);
  static google::protobuf::Value MakeValueProto(Numeric n);
  static google::protobuf::Value MakeValueProto(Timestamp ts);
  static google::protobuf::Value MakeValueProto(CommitTimestamp ts);
  static google::protobuf::Value MakeValueProto(absl::CivilDay d);
  static google::protobuf::Value MakeValueProto(int i);
  static google::protobuf::Value MakeValueProto(char const* s);
  template <typename T>
  static google::protobuf::Value MakeValueProto(absl::optional<T> opt) {
    if (opt.has_value()) return MakeValueProto(*std::move(opt));
    google::protobuf::Value v;
    v.set_null_value(google::protobuf::NullValue::NULL_VALUE);
    return v;
  }
  template <typename T>
  static google::protobuf::Value MakeValueProto(std::vector<T> vec) {
    google::protobuf::Value v;
    auto& list = *v.mutable_list_value();
    for (auto&& e : vec) {
      *list.add_values() = MakeValueProto(std::move(e));
    }
    return v;
  }
  template <typename... Ts>
  static google::protobuf::Value MakeValueProto(std::tuple<Ts...> tup) {
    google::protobuf::Value v;
    spanner_internal::ForEach(tup, AddStructValues{}, *v.mutable_list_value());
    return v;
  }

  // A functor to be used with internal::ForEach to add Value protos for all
  // the elements of a tuple.
  struct AddStructValues {
    template <typename T>
    void operator()(T& t, google::protobuf::ListValue& list_value) const {
      *list_value.add_values() = MakeValueProto(std::move(t));
    }
    template <typename S, typename T,
              typename std::enable_if<
                  std::is_convertible<S, std::string>::value, int>::type = 0>
    void operator()(std::pair<S, T> p,
                    google::protobuf::ListValue& list_value) const {
      *list_value.add_values() = MakeValueProto(std::move(p.second));
    }
  };

  // Tag-dispatch overloads to extract a C++ value from a `Value` protobuf. The
  // first argument type is the tag, its value is ignored.
  static StatusOr<bool> GetValue(bool, google::protobuf::Value const&,
                                 google::spanner::v1::Type const&);
  static StatusOr<std::int64_t> GetValue(std::int64_t,
                                         google::protobuf::Value const&,
                                         google::spanner::v1::Type const&);
  static StatusOr<double> GetValue(double, google::protobuf::Value const&,
                                   google::spanner::v1::Type const&);
  static StatusOr<std::string> GetValue(std::string const&,
                                        google::protobuf::Value const&,
                                        google::spanner::v1::Type const&);
  static StatusOr<std::string> GetValue(std::string const&,
                                        google::protobuf::Value&&,
                                        google::spanner::v1::Type const&);
  static StatusOr<Bytes> GetValue(Bytes const&, google::protobuf::Value const&,
                                  google::spanner::v1::Type const&);
  static StatusOr<Numeric> GetValue(Numeric const&,
                                    google::protobuf::Value const&,
                                    google::spanner::v1::Type const&);
  static StatusOr<Timestamp> GetValue(Timestamp, google::protobuf::Value const&,
                                      google::spanner::v1::Type const&);
  static StatusOr<CommitTimestamp> GetValue(CommitTimestamp,
                                            google::protobuf::Value const&,
                                            google::spanner::v1::Type const&);
  static StatusOr<absl::CivilDay> GetValue(absl::CivilDay,
                                           google::protobuf::Value const&,
                                           google::spanner::v1::Type const&);
  template <typename T, typename V>
  static StatusOr<absl::optional<T>> GetValue(
      absl::optional<T> const&, V&& pv, google::spanner::v1::Type const& pt) {
    if (pv.kind_case() == google::protobuf::Value::kNullValue) {
      return absl::optional<T>{};
    }
    auto value = GetValue(T{}, std::forward<V>(pv), pt);
    if (!value) return std::move(value).status();
    return absl::optional<T>{*std::move(value)};
  }
  template <typename T, typename V>
  static StatusOr<std::vector<T>> GetValue(
      std::vector<T> const&, V&& pv, google::spanner::v1::Type const& pt) {
    if (pv.kind_case() != google::protobuf::Value::kListValue) {
      return Status(StatusCode::kUnknown, "missing ARRAY");
    }
    std::vector<T> v;
    for (int i = 0; i < pv.list_value().values().size(); ++i) {
      auto&& e = GetProtoListValueElement(std::forward<V>(pv), i);
      using ET = decltype(e);
      auto value = GetValue(T{}, std::forward<ET>(e), pt.array_element_type());
      if (!value) return std::move(value).status();
      v.push_back(*std::move(value));
    }
    return v;
  }
  template <typename V, typename... Ts>
  static StatusOr<std::tuple<Ts...>> GetValue(
      std::tuple<Ts...> const&, V&& pv, google::spanner::v1::Type const& pt) {
    if (pv.kind_case() != google::protobuf::Value::kListValue) {
      return Status(StatusCode::kUnknown, "missing STRUCT");
    }
    std::tuple<Ts...> tup;
    Status status;  // OK
    ExtractTupleValues<V> f{status, 0, std::forward<V>(pv), pt};
    spanner_internal::ForEach(tup, f);
    if (!status.ok()) return status;
    return tup;
  }

  // A functor to be used with internal::ForEach to extract C++ types from a
  // ListValue proto and store then in a tuple.
  template <typename V>
  struct ExtractTupleValues {
    Status& status;
    int i;
    V&& pv;
    google::spanner::v1::Type const& type;
    template <typename T>
    void operator()(T& t) {
      auto&& e = GetProtoListValueElement(std::forward<V>(pv), i);
      using ET = decltype(e);
      auto value = GetValue(T{}, std::forward<ET>(e), type);
      ++i;
      if (!value) {
        status = std::move(value).status();
      } else {
        t = *std::move(value);
      }
    }
    template <typename T>
    void operator()(std::pair<std::string, T>& p) {
      p.first = type.struct_type().fields(i).name();
      auto&& e = GetProtoListValueElement(std::forward<V>(pv), i);
      using ET = decltype(e);
      auto value = GetValue(T{}, std::forward<ET>(e), type);
      ++i;
      if (!value) {
        status = std::move(value).status();
      } else {
        p.second = *std::move(value);
      }
    }
  };

  // Protocol buffers are not friendly to generic programming, because they use
  // different syntax and different names for mutable and non-mutable
  // functions. To make GetValue(vector<T>, ...) (above) work, we need split
  // the different protobuf syntaxes into overloaded functions.
  static google::protobuf::Value const& GetProtoListValueElement(
      google::protobuf::Value const& pv, int pos) {
    return pv.list_value().values(pos);
  }
  static google::protobuf::Value&& GetProtoListValueElement(
      google::protobuf::Value&& pv, int pos) {
    return std::move(*pv.mutable_list_value()->mutable_values(pos));
  }

  // A private templated constructor that is called by all the public
  // constructors to set the type_ and value_ members. The `PrivateConstructor`
  // type is used so that this overload is never chosen for
  // non-member/non-friend callers. Otherwise, since visibility restrictions
  // apply after overload resolution, users could get weird error messages if
  // this constructor matched their arguments best.
  struct PrivateConstructor {};
  template <typename T>
  Value(PrivateConstructor, T&& t)
      : type_(MakeTypeProto(t)), value_(MakeValueProto(std::forward<T>(t))) {}

  Value(google::spanner::v1::Type t, google::protobuf::Value v)
      : type_(std::move(t)), value_(std::move(v)) {}

  friend struct spanner_internal::SPANNER_CLIENT_NS::ValueInternals;

  google::spanner::v1::Type type_;
  google::protobuf::Value value_;
};

/**
 * Factory to construct a "null" Value of the specified type `T`.
 *
 * This is equivalent to passing an `absl::optional<T>` without a value to
 * the constructor, though this factory may be easier to invoke and result
 * in clearer code at the call site.
 */
template <typename T>
Value MakeNullValue() {
  return Value(absl::optional<T>{});
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

struct ValueInternals {
  static spanner::Value FromProto(google::spanner::v1::Type t,
                                  google::protobuf::Value v) {
    return spanner::Value(std::move(t), std::move(v));
  }

  static std::pair<google::spanner::v1::Type, google::protobuf::Value> ToProto(
      spanner::Value v) {
    return std::make_pair(std::move(v.type_), std::move(v.value_));
  }
};

inline spanner::Value FromProto(google::spanner::v1::Type t,
                                google::protobuf::Value v) {
  return ValueInternals::FromProto(std::move(t), std::move(v));
}

inline std::pair<google::spanner::v1::Type, google::protobuf::Value> ToProto(
    spanner::Value v) {
  return ValueInternals::ToProto(std::move(v));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H
