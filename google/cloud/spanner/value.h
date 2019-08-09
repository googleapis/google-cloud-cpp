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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_VALUE_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_VALUE_H_

#include "google/cloud/spanner/date.h"
#include "google/cloud/spanner/internal/tuple_utils.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/spanner/v1/type.pb.h>
#include <chrono>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class Value;  // Defined later in this file.
// Internal implementation details that callers should not use.
namespace internal {
Value FromProto(google::spanner::v1::Type t, google::protobuf::Value v);
std::pair<google::spanner::v1::Type, google::protobuf::Value> ToProto(Value v);
}  // namespace internal

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
 * BYTES        | `Value::Bytes`
 * TIMESTAMP    | `google::cloud::spanner::Timestamp`
 * DATE         | `google::cloud::spanner::Date`
 * ARRAY        | `std::vector<T>`  // [1]
 * STRUCT       | `std::tuple<Ts...>`
 *
 * [1] The type `T` may be any of the other supported types, except for
 *     ARRAY/`std::vector`.
 *
 * Value is a regular C++ value type with support for copy, move, equality,
 * etc. A default-constructed Value represents an empty value with no type.
 *
 * Callers may create instances by passing any of the supported values (shown
 * in the table above) to the constructor. "Null" values are created using the
 * `MakeNullValue<T>()` factory function or by passing an empty `optional<T>`
 * to the Value constructor..
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
 * assert(!i.ok());  // Can't get the value becuase v is null
 * StatusOr<optional<std::int64_t> j = v.get<optional<std::int64_t>>();
 * assert(j.ok());  // OK because an empty option can represent the null
 * assert(!j->has_value());  // v held no value.
 * @endcode
 *
 * @par Nullness
 *
 * All of the supported types (above) are "nullable". A null is created in one
 * of two ways:
 *
 * 1. Passing an `optional<T>()` with no value to `Value`'s constructor.
 * 2. Using the `MakeNullValue<T>()` helper function (defined below).
 *
 * Nulls can be retrieved from a `Value::get<T>` by specifying the type `T` as
 * a `optional<U>`. The returned optional will either be empty (indicating
 * null) or it will contain the actual value. See the documentation for
 * `Value::get<T>` below for more details.
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
 * pair's `.first` member indicate's the field's name, and the `.second` member
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
  explicit Value(Timestamp v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(Date v) : Value(PrivateConstructor{}, std::move(v)) {}

  /**
   * A struct that represents a collection of bytes.
   *
   * This struct is a thin wrapper around a `std::string` to distinguish BYTES
   * from STRINGs. The `.data` member can be set/get directly. Consructors and
   * relational operators are provided for convenience and to make `Bytes` a
   * regular type that is easy to work with.
   *
   * Note that the relational operators behave as if the `std::string` chars
   * are unsigned, which is exactly how Spanner BYTES values compare.
   */
  struct Bytes {
    std::string data;

    /// Constructs a Bytes struct from the given string.
    explicit Bytes(std::string s) : data(std::move(s)) {}

    /// @name Copy and move
    ///@{
    Bytes() = default;
    Bytes(Bytes const&) = default;
    Bytes(Bytes&&) = default;
    Bytes& operator=(Bytes const&) = default;
    Bytes& operator=(Bytes&&) = default;
    ///@}

    /// @name Relational operators
    ///@{
    friend bool operator==(Bytes const& a, Bytes const& b) {
      return a.data == b.data;
    }
    friend bool operator!=(Bytes const& a, Bytes const& b) { return !(a == b); }
    friend bool operator<(Bytes const& a, Bytes const& b) {
      return a.data < b.data;
    }
    friend bool operator<=(Bytes const& a, Bytes const& b) { return !(b < a); }
    friend bool operator>=(Bytes const& a, Bytes const& b) { return !(a < b); }
    friend bool operator>(Bytes const& a, Bytes const& b) { return b < a; }
    ///@}
  };
  /// Constructs an instance with the specicified bytes.
  explicit Value(Bytes v) : Value(PrivateConstructor{}, std::move(v)) {}

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
  explicit Value(optional<T> opt)
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
    static_assert(!is_vector<typename std::decay<T>::type>::value,
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
   * * The contained value is "null", and `T` is not an `optional`.
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
   *
   * StatusOr<optional<std::int64_t>> j = v.get<optional<std::int64_t>();
   * assert(j.ok());  // Since we know the types match in this example
   * assert(!v->has_value());  // Since we know v was null in this example
   * @endcode
   */
  template <typename T>
  StatusOr<T> get() const {
    // Ignores the name field because it is never set on the incoming `T`.
    if (!EqualTypeProtoIgnoringNames(type_, MakeTypeProto(T{})))
      return Status(StatusCode::kUnknown, "wrong type");
    if (value_.kind_case() == google::protobuf::Value::kNullValue) {
      if (is_optional<T>::value) return T{};
      return Status(StatusCode::kUnknown, "null value");
    }
    return GetValue(T{}, value_, type_);
  }

  /**
   * Allows Google Test to print internal debugging information when test
   * assertions fail.
   *
   * @warning This is intended for debugging and human consumption only, not
   *   machine consumption as the output format may change without notice.
   */
  friend void PrintTo(Value const& v, std::ostream* os);

 private:
  // Metafunction that returns true if `T` is an optional<U>
  template <typename T>
  struct is_optional : std::false_type {};
  template <typename T>
  struct is_optional<optional<T>> : std::true_type {};

  // Metafunction that returns true if `T` is a std::vector<U>
  template <typename T>
  struct is_vector : std::false_type {};
  template <typename... Ts>
  struct is_vector<std::vector<Ts...>> : std::true_type {};

  // Tag-dispatch overloads to convert a C++ type to a `Type` protobuf. The
  // argument type is the tag, the argument value is ignored.
  static google::spanner::v1::Type MakeTypeProto(bool);
  static google::spanner::v1::Type MakeTypeProto(std::int64_t);
  static google::spanner::v1::Type MakeTypeProto(double);
  static google::spanner::v1::Type MakeTypeProto(std::string const&);
  static google::spanner::v1::Type MakeTypeProto(Bytes const&);
  static google::spanner::v1::Type MakeTypeProto(Timestamp);
  static google::spanner::v1::Type MakeTypeProto(Date);
  static google::spanner::v1::Type MakeTypeProto(int);
  static google::spanner::v1::Type MakeTypeProto(char const*);
  template <typename T>
  static google::spanner::v1::Type MakeTypeProto(optional<T> const&) {
    return MakeTypeProto(T{});
  }
  template <typename T>
  static google::spanner::v1::Type MakeTypeProto(std::vector<T> const& v) {
    google::spanner::v1::Type t;
    t.set_code(google::spanner::v1::TypeCode::ARRAY);
    *t.mutable_array_element_type() = MakeTypeProto(v.empty() ? T{} : v[0]);
    // Checks that vector elements have exactly the same proto Type, which
    // includes field names. This is documented UB.
    for (auto const& e : v) {
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
    internal::ForEach(tup, AddStructTypes{}, *t.mutable_struct_type());
    return t;
  }

  // A functor to be used with internal::ForEach (see below) to add type protos
  // for all the elements of a tuple.
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
  static google::protobuf::Value MakeValueProto(Bytes const& b);
  static google::protobuf::Value MakeValueProto(Timestamp ts);
  static google::protobuf::Value MakeValueProto(Date d);
  static google::protobuf::Value MakeValueProto(int i);
  static google::protobuf::Value MakeValueProto(char const* s);
  template <typename T>
  static google::protobuf::Value MakeValueProto(optional<T> opt) {
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
    internal::ForEach(tup, AddStructValues{}, *v.mutable_list_value());
    return v;
  }

  // A functor to be used with internal::ForEach (see below) to add Value
  // protos for all the elements of a tuple.
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
  // first argument type is the tag, the first argument value is ignored.
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
  static StatusOr<Bytes> GetValue(Bytes const&, google::protobuf::Value const&,
                                  google::spanner::v1::Type const&);
  static StatusOr<Timestamp> GetValue(Timestamp, google::protobuf::Value const&,
                                      google::spanner::v1::Type const&);
  static StatusOr<Date> GetValue(Date, google::protobuf::Value const&,
                                 google::spanner::v1::Type const&);
  template <typename T>
  static StatusOr<optional<T>> GetValue(optional<T> const&,
                                        google::protobuf::Value const& pv,
                                        google::spanner::v1::Type const& pt) {
    if (pv.kind_case() == google::protobuf::Value::kNullValue) {
      return optional<T>{};
    }
    auto value = GetValue(T{}, pv, pt);
    if (!value) return std::move(value).status();
    return optional<T>{*std::move(value)};
  }
  template <typename T>
  static StatusOr<std::vector<T>> GetValue(
      std::vector<T> const&, google::protobuf::Value const& pv,
      google::spanner::v1::Type const& pt) {
    if (pv.kind_case() != google::protobuf::Value::kListValue) {
      return Status(StatusCode::kUnknown, "missing ARRAY");
    }
    std::vector<T> v;
    for (auto const& e : pv.list_value().values()) {
      auto value = GetValue(T{}, e, pt.array_element_type());
      if (!value) return std::move(value).status();
      v.push_back(*std::move(value));
    }
    return v;
  }
  template <typename... Ts>
  static StatusOr<std::tuple<Ts...>> GetValue(
      std::tuple<Ts...> const&, google::protobuf::Value const& pv,
      google::spanner::v1::Type const& pt) {
    if (pv.kind_case() != google::protobuf::Value::kListValue) {
      return Status(StatusCode::kUnknown, "missing STRUCT");
    }
    std::tuple<Ts...> tup;
    Status status;  // OK
    ExtractTupleValues f{status, 0, pv.list_value(), pt};
    internal::ForEach(tup, f);
    if (!status.ok()) return status;
    return tup;
  }

  // A functor to be used with internal::ForEach (see below) to extract C++
  // types from a ListValue proto and store then in a tuple.
  struct ExtractTupleValues {
    Status& status;
    std::size_t i;
    google::protobuf::ListValue const& list_value;
    google::spanner::v1::Type const& type;
    template <typename T>
    void operator()(T& t) {
      auto value = GetValue(T{}, list_value.values(i), type);
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
      auto value = GetValue(T{}, list_value.values(i), type);
      ++i;
      if (!value) {
        status = std::move(value).status();
      } else {
        p.second = *std::move(value);
      }
    }
  };

  static bool EqualTypeProtoIgnoringNames(google::spanner::v1::Type const& a,
                                          google::spanner::v1::Type const& b);

  // A private templated constructor that is called by all the public
  // constructors to set the type_ and value_ members. The `PrivateConstructor`
  // type is used so that this overload is never chosen for
  // non-member/non-friend callers. Otherwise, since visibility restrictions
  // apply after overload resolution, users could get weird error messages if
  // this constructor matched their arguments best.
  struct PrivateConstructor {};
  template <typename T>
  explicit Value(PrivateConstructor, T&& t)
      : type_(MakeTypeProto(t)), value_(MakeValueProto(std::forward<T>(t))) {}

  explicit Value(google::spanner::v1::Type t, google::protobuf::Value v)
      : type_(std::move(t)), value_(std::move(v)) {}

  friend Value internal::FromProto(google::spanner::v1::Type,
                                   google::protobuf::Value);
  friend std::pair<google::spanner::v1::Type, google::protobuf::Value>
      internal::ToProto(Value);

  google::spanner::v1::Type type_;
  google::protobuf::Value value_;
};

/**
 * Factory to construct a "null" Value of the specified type `T`.
 *
 * This is equivalent to passing an `optional<T>` without a value to the
 * constructor, though this factory may be easier to invoke and result in
 * clearer code at the call site.
 */
template <typename T>
Value MakeNullValue() {
  return Value(optional<T>{});
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_VALUE_H_
