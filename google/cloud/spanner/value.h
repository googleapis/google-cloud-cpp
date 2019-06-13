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

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/optional.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/field_comparator.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/spanner/v1/type.pb.h>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

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
 * Spanner Type | C++ Type `T`
 * ------------ | ------------
 * BOOL         | `bool`
 * INT64        | `std::int64_t`
 * FLOAT64      | `double`
 * STRING       | `std::string`
 * ARRAY        | `std::vector<T>`  // [1]
 * STRUCT       | `std::tuple<Ts...>`
 *
 * [1] The type `T` may be any of the other supported types, except for
 *     ARRAY/`std::vector`.
 *
 * Value is a regular C++ value type with support for copy, move, equality,
 * etc, but there is no default constructor because there is no default type.
 * Callers may create instances by passing any of the supported values (shown
 * in the table above) to the constructor. "Null" values are created using the
 * `MakeNullValue<T>()` factory function or by passing an empty `optional<T>`
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
 *     spanner::Value v = spanner::MakeNullValue<std::int64_t>();
 *     assert(v.is<std::int64_t>());
 *     assert(v.is_null<std::int64_t>());
 *     StatusOr<std::int64_t> i = v.get<std::int64_t>();
 *     assert(!i.ok());  // Can't get the value becuase v is null
 *     StatusOr<optional<std::int64_t> j = v.get<optional<std::int64_t>>();
 *     assert(j.ok());  // OK because an empty option can represent the null
 *
 * # Spanner Arrays (i.e., `std::vector<T>`)
 *
 * Spanner arrays are represented in C++ as a `std::vector<T>`, where the type
 * `T` may be any of the other allowed Spanner types, such as `bool`,
 * `std::int64_t`, etc. The only exception is that arrays may not directly
 * contain another array; to achieve a similar result you could create an array
 * of a 1-element struct holding an array. The following examples show usage of
 * arrays.
 *
 *     std::vector<std::int64_t> vec = {1, 2, 3, 4, 5};
 *     spanner::Value v(vec);
 *     assert(v.is<std::vector<std::int64_t>>());
 *     auto copy = *v.get<std::vector<std::int64_t>>();
 *     assert(vec == copy);
 *
 * # Spanner Structs (i.e., `std::tuple<Ts...>`)
 *
 * Spanner structs are represented in C++ as instances of `std::tuple` holding
 * zero or more of the allowed Spanner types, such as `bool`, `std::int64_t`,
 * `std::vector`, and even other `std::tuple` objects. Each tuple element
 * corresponds to a single field in a Spanner Struct.
 *
 * Spanner struct fields may optionally contain a string indicating the field's
 * name. Fields names may be empty, unique, or repeated. A named field may be
 * specified as a tuple element of type `std::pair<std::string, T>`, where the
 * pair's `.first` member indicate's the field's name, and the `.second` member
 * is any valid Spanner type `T`.
 *
 *     using Struct = std::tuple<bool, std::pair<std::string, std::int64_t>>;
 *     Struct s  = {true, {"Foo", 42}};
 *     spanner::Value v(s);
 *     assert(v.is<Struct>());
 *     assert(s == *v.get<Struct>());
 *
 * NOTE: While a struct's (optional) field names are not part of its C++ type,
 * they are part of its Spanner struct type. Array's (i.e., `std::vector`)
 * must contain a single element type, therefore it is an error to construct
 * a `std::vector` of `std::tuple` objects with differently named fields.
 */
class Value {
 public:
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
    type_ = MakeTypeProto(T{});
    value_ = MakeValueProto(opt);
  }

  /**
   * Constructs an instance from a Spanner ARRAY of the specified type and
   * values.
   *
   * The type `T` may be any valid type shown above, except vectors of vectors
   * are not allowed.
   *
   * NOTE: If `T` is a `std::tuple` with field names (i.e., at least one of its
   * element types is a `std::pair<std::string, T>`) then, all of the vector's
   * elements must have exactly the same field names. Any mismatch in in field
   * names results in undefined bhavior.
   */
  template <typename T>  // TODO(#59): add an enabler to disallow T==vector
  explicit Value(std::vector<T> const& v) {
    type_ = MakeTypeProto(v);
    value_ = MakeValueProto(v);
  }

  /**
   * Constructs an instance from a Spanner STRUCT with a type and values
   * matching the given `std::tuple`.
   *
   * Any struct field may optionally have a name, which is specified as
   * `std::pair<std::string, T>`.
   */
  template <typename... Ts>
  explicit Value(std::tuple<Ts...> const& tup) {
    type_ = MakeTypeProto(tup);
    value_ = MakeValueProto(tup);
  }

  friend bool operator==(Value const& a, Value const& b);
  friend bool operator!=(Value const& a, Value const& b) { return !(a == b); }

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
   *     spanner::Value null_v = spanner::MakeNullValue<bool>();
   *     assert(v.is<bool>());  // Same as the following
   *     assert(v.is<optional<bool>>());
   */
  template <typename T>
  bool is() const {
    google::protobuf::util::MessageDifferencer diff;
    auto const* field = google::spanner::v1::StructType::Field::descriptor();
    // Ignores the name field because it is never set on the incoming `T`.
    diff.IgnoreField(field->FindFieldByName("name"));
    return diff.Compare(type_, MakeTypeProto(T{}));
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
   *     spanner::Value null_v = spanner::MakeNullValue<bool>();
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
   *     v = spanner::MakeNullValue<std::int64_t>();
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
    return GetValue(T{}, value_, type_);
  }

  /**
   * Returns the contained value iff `is<T>()` and `!is_null<T>()`.
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
   *     spanner::Value null_v = spanner::MakeNullValue<std::int64_t>();
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
  static google::spanner::v1::Type MakeTypeProto(bool);
  static google::spanner::v1::Type MakeTypeProto(std::int64_t);
  static google::spanner::v1::Type MakeTypeProto(double);
  static google::spanner::v1::Type MakeTypeProto(std::string const&);
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
        internal::ThrowInvalidArgument("Mismatched types");
      }
    }
    return t;
  }
  template <typename... Ts>
  static google::spanner::v1::Type MakeTypeProto(std::tuple<Ts...> const& tup) {
    google::spanner::v1::Type t;
    t.set_code(google::spanner::v1::TypeCode::STRUCT);
    IterateTuple(tup, AddStructTypes{}, *t.mutable_struct_type());
    return t;
  }

  // A functor to be used with IterateTuple (see below) to add type protos for
  // all the elements of a tuple.
  struct AddStructTypes {
    template <typename T>
    void operator()(T const& t,
                    google::spanner::v1::StructType& struct_type) const {
      auto* field = struct_type.add_fields();
      *field->mutable_type() = MakeTypeProto(t);
    }
    template <typename T>
    void operator()(std::pair<std::string, T> const& p,
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
  template <typename T>
  static google::protobuf::Value MakeValueProto(optional<T> const& opt) {
    if (opt.has_value()) return MakeValueProto(*opt);
    google::protobuf::Value v;
    v.set_null_value(google::protobuf::NullValue::NULL_VALUE);
    return v;
  }
  template <typename T>
  static google::protobuf::Value MakeValueProto(std::vector<T> const& vec) {
    google::protobuf::Value v;
    for (auto const& e : vec) {
      *v.mutable_list_value()->add_values() = MakeValueProto(e);
    }
    return v;
  }
  template <typename... Ts>
  static google::protobuf::Value MakeValueProto(std::tuple<Ts...> const& tup) {
    google::protobuf::Value v;
    IterateTuple(tup, AddStructValues{}, *v.mutable_list_value());
    return v;
  }

  // A functor to be used with IterateTuple (see below) to add Value protos for
  // all the elements of a tuple.
  struct AddStructValues {
    template <typename T>
    void operator()(T const& t, google::protobuf::ListValue& list_value) const {
      *list_value.add_values() = MakeValueProto(t);
    }
    template <typename T>
    void operator()(std::pair<std::string, T> const& p,
                    google::protobuf::ListValue& list_value) const {
      *list_value.add_values() = MakeValueProto(p.second);
    }
  };

  // Tag-dispatch overloads to extract a C++ value from a `Value` protobuf. The
  // first argument type is the tag, the first argument value is ignored.
  static bool GetValue(bool, google::protobuf::Value const&,
                       google::spanner::v1::Type const&);
  static std::int64_t GetValue(std::int64_t, google::protobuf::Value const&,
                               google::spanner::v1::Type const&);
  static double GetValue(double, google::protobuf::Value const&,
                         google::spanner::v1::Type const&);
  static std::string GetValue(std::string const&,
                              google::protobuf::Value const&,
                              google::spanner::v1::Type const&);
  template <typename T>
  static optional<T> GetValue(optional<T> const&,
                              google::protobuf::Value const& pv,
                              google::spanner::v1::Type const& pt) {
    return GetValue(T{}, pv, pt);
  }
  template <typename T>
  static std::vector<T> GetValue(std::vector<T> const&,
                                 google::protobuf::Value const& pv,
                                 google::spanner::v1::Type const& pt) {
    std::vector<T> v;
    for (auto const& e : pv.list_value().values()) {
      v.push_back(GetValue(T{}, e, pt.array_element_type()));
    }
    return v;
  }
  template <typename... Ts>
  static std::tuple<Ts...> GetValue(std::tuple<Ts...> const&,
                                    google::protobuf::Value const& pv,
                                    google::spanner::v1::Type const& pt) {
    std::tuple<Ts...> tup;
    ExtractTupleValues f{0, pv.list_value(), pt};
    IterateTuple(tup, f);
    return tup;
  }

  // A functor to be used with IterateTuple (see below) to extract C++ types
  // from a ListValue proto and store then in a tuple.
  struct ExtractTupleValues {
    std::size_t i;
    google::protobuf::ListValue const& list_value;
    google::spanner::v1::Type const& type;
    template <typename T>
    void operator()(T& t) {
      t = GetValue(T{}, list_value.values(i), type);
      ++i;
    }
    template <typename T>
    void operator()(std::pair<std::string, T>& p) {
      p.first = type.struct_type().fields(i).name();
      p.second = GetValue(T{}, list_value.values(i), type);
      ++i;
    }
  };

  // A helper to iterate the elements of the tuple, calling the given functor
  // `f` with each tuple element and any optional `args`. Typically `F` should
  // be a functor type with a templated operator() so that it can handle the
  // various types in the tuple.
  template <std::size_t I = 0, typename Tup, typename F, typename... Args>
  static typename std::enable_if<
      (I < std::tuple_size<typename std::decay<Tup>::type>::value), void>::type
  IterateTuple(Tup&& tup, F&& f, Args&&... args) {
    auto&& e = std::get<I>(std::forward<Tup>(tup));
    std::forward<F>(f)(std::forward<decltype(e)>(e),
                       std::forward<Args>(args)...);
    IterateTuple<I + 1>(std::forward<Tup>(tup), std::forward<F>(f),
                        std::forward<Args>(args)...);
  }
  template <std::size_t I = 0, typename Tup, typename F, typename... Args>
  static typename std::enable_if<
      I == std::tuple_size<typename std::decay<Tup>::type>::value, void>::type
  IterateTuple(Tup&&, F&&, Args&&...) {}

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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_VALUE_H_
