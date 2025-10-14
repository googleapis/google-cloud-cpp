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
#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VALUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VALUE_H

#include "google/cloud/bigtable/internal/tuple_utils.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"
#include "bytes.h"
#include "timestamp.h"
#include <google/bigtable/v2/data.pb.h>
#include <google/bigtable/v2/types.pb.h>
#include <cmath>
#include <vector>

namespace google {
namespace cloud {

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct ValueInternals;

static std::string const kInvalidFloatValueMessage =
    "NaN and Infinity are not supported for FLOAT** values";

static bool validate_float_value(double v) {
  if (std::isnan(v) || std::isinf(v)) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    throw internal::FailedPreconditionError(kInvalidFloatValueMessage);
#else   // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    return false;
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  }
  return true;
}

static bool ValidateFloatValue(double v) { return validate_float_value(v); }

static bool ValidateFloatValue(float v) { return validate_float_value(v); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal

namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The Value class represents a type-safe, nullable Bigtable value.
 *
 * For now, this is a minimal class to unblock further development of the
 * Bigtable GoogleSQL feature.
 * It is conceptually similar to a `std::any` except the only allowed types will
 * be those supported by Bigtable, and a "null" value (similar to a `std::any`
 * without a value) still has an associated type. The supported types are shown
 * in the following table along with how they map to the Bigtable types
 * (https://cloud.google.com/bigtable/docs/data-types):
 * Bigtable Type | C++ Type `T`
 * ------------ | ------------
 * BOOL         | `bool`
 * INT64        | `std::int64_t`
 * FLOAT32      | `float`
 * FLOAT64      | `double`
 * STRING       | `std::string`
 * BYTES        | `google::cloud::bigtable::Bytes`
 * TIMESTAMP    | `google::cloud::bigtable::Timestamp`
 * DATE         | `absl::CivilDay`
 * ARRAY        | `std::vector<T>`  // [1]
 * STRUCT       | `std::tuple<Ts...>`
 * MAP          | `std::unordered_map<K, V>` // [2]
 *
 * [1] The type `T` may be any of the other supported types, except for
 *     ARRAY/`std::vector`.
 * [2] The type `K` may be any of `Bytes`, `std::string`, and `std::int64_t`.
 *
 *
 * Callers may create instances by passing any of the supported values
 * (shown in the table above) to the constructor. "Null" values are created
 * using the `MakeNullValue<T>()` factory function or by passing an empty
 * `absl::optional<T>` to the Value constructor.
 *
 * @par Bigtable Arrays
 *
 * Bigtable arrays are represented in C++ as a `std::vector<T>`, where the type
 * `T` may be any of the other allowed Bigtable types, such as `bool`,
 * `std::int64_t`, etc. The only exception is that arrays may not directly
 * contain another array; to achieve a similar result you could create an array
 * of a 1-element struct holding an array. The following examples show usage of
 * arrays.
 *
 * @code
 * std::vector<std::int64_t> vec = {1, 2, 3, 4, 5};
 * bigtable::Value v(vec);
 * auto copy = *v.get<std::vector<std::int64_t>>();
 * assert(vec == copy);
 * @endcode
 *
 * @par Bigtable Structs
 *
 * Bigtable structs are represented in C++ as instances of `std::tuple` holding
 * zero or more of the allowed Bigtable types, such as `bool`, `std::int64_t`,
 * `std::vector`, and even other `std::tuple` objects. Each tuple element
 * corresponds to a single field in a Bigtable STRUCT.
 *
 * Bigtable STRUCT fields may optionally contain a string indicating the field's
 * name. Fields names may be empty, unique, or repeated. A named field may be
 * specified as a tuple element of type `std::pair<std::string, T>`, where the
 * pair's `.first` member indicates the field's name, and the `.second` member
 * is any valid Bigtable type `T`.
 *
 * @code
 * using Struct = std::tuple<bool, std::pair<std::string, std::int64_t>>;
 * Struct s  = {true, {"Foo", 42}};
 * bigtable::Value v(s);
 * assert(s == *v.get<Struct>());
 * @endcode
 *
 * @note While a STRUCT's (optional) field names are not part of its C++ type,
 *   they are part of its Bigtable STRUCT type. Array's (i.e., `std::vector`)
 *   must contain a single element type, therefore it is an error to construct
 *   a `std::vector` of `std::tuple` objects with differently named fields.
 *
 * @par Bigtable Maps
 *
 * Bigtable maps are represented in C++ as a `std::unordered_map<K, V>`, where
 * the type `K` may be any of `Bytes`, `std::string` or `std::int64_t`. Normally
 * encoded Map values won't have repeated keys, however, this client handles the
 * case as follows: if the same key appears multiple times, the _last_ value
 * takes precedence.
 *
 * The following examples show usage of maps.
 *
 * @code
 * std::unordered_map<std::string, std::unordered_map<std::string,
 * std::int64_t>> m = {{"map1",
 * {{"key1", 1}}, "map2": {{"key2", 2}}};
 * bigtable::Value mv(m);
 * auto copy = *v.get<std::unordered_map<std::string, std::int64_t>>>();
 * assert(m == copy);
 * @endcode
 */
class Value {
 public:
  /**
   * Constructs a `Value` that holds nothing.
   */
  Value() = default;
  /// Constructs an instance with the specified type and value.
  explicit Value(bool v) : Value(PrivateConstructor{}, v) {}
  /// @copydoc Value(bool)
  explicit Value(std::int64_t v) : Value(PrivateConstructor{}, v) {}
  /// @copydoc Value(bool)
  explicit Value(float v) : Value(PrivateConstructor{}, v) {
    bigtable_internal::ValidateFloatValue(v);
  }
  /// @copydoc Value(bool)
  explicit Value(double v) : Value(PrivateConstructor{}, v) {
    bigtable_internal::ValidateFloatValue(v);
  }
  /// @copydoc Value(bool)v)
  explicit Value(std::string v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(Bytes const& v) : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(Timestamp const& v)
      : Value(PrivateConstructor{}, std::move(v)) {}
  /// @copydoc Value(bool)
  explicit Value(absl::CivilDay const& v)
      : Value(PrivateConstructor{}, std::move(v)) {}
  /**
   * Constructs an instance from common C++ literal types that closely, though
   * not exactly, match supported Bigtable types.
   *
   * An integer literal in C++ is of type `int`, which is not exactly an
   * allowed Bigtable type. This will be allowed but it will be implicitly up
   * converted to a `std::int64_t`. Similarly, a C++ string literal will be
   * implicitly converted to a `std::string`. For example:
   *
   * @code
   * bigtable::Value v1(42);
   * assert(42 == *v1.get<std::int64_t>());
   *
   * bigtable::Value v2("hello");
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
   * Constructs an instance from a Bigtable ARRAY of the specified type and
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
    static_assert(!IsVector<std::decay_t<T>>::value,
                  "vector of vector not allowed. See value.h documentation.");
  }

  /**
   * Constructs an instance from a Bigtable STRUCT with a type and values
   * matching the given `std::tuple`.
   *
   * Any STRUCT field may optionally have a name, which is specified as
   * `std::pair<std::string, T>`.
   */
  template <typename... Ts>
  explicit Value(std::tuple<Ts...> tup)
      : Value(PrivateConstructor{}, std::move(tup)) {}

  /**
   * Constructs an instance from a Bigtable MAP with a type and values
   * matching the given `std::unordered_map`.
   *
   * @warning if the same key appears
   * multiple times, the _last_ value takes precedence.
   */
  template <typename K, typename V>
  explicit Value(std::unordered_map<K, V> m)
      : Value(PrivateConstructor{}, std::move(m)) {
    static_assert(IsValidMapKey<K>::value,
                  "Invalid key type. See value.h documentation.");
  }

  // Copy and move.
  Value(Value const&) = default;
  Value(Value&&) = default;
  Value& operator=(Value const&) = default;
  Value& operator=(Value&&) = default;

  // The public API is declared, but the implementations can be deferred.
  // Dependent code can be compiled against these declarations.

  template <typename T>
  StatusOr<T> get() const& {
    if (!TypeProtoIs(T{}, type_))
      return internal::UnknownError("wrong type", GCP_ERROR_INFO());
    if (is_null()) {
      if (IsOptional<T>::value) return T{};
      return internal::UnknownError("null value", GCP_ERROR_INFO());
    }
    return GetValue(T{}, value_, type_);
  }

  /// @copydoc get()
  template <typename T>
  StatusOr<T> get() && {
    if (!TypeProtoIs(T{}, type_))
      return internal::UnknownError("wrong type", GCP_ERROR_INFO());
    if (is_null()) {
      if (IsOptional<T>::value) return T{};
      return internal::UnknownError("null value", GCP_ERROR_INFO());
    }
    auto tag = T{};  // Works around an odd msvc issue
    return GetValue(std::move(tag), std::move(value_), type_);
  }

  google::bigtable::v2::Type const& type() const& { return type_; }

  bool is_null() const;

  // Equality operators are part of the interface, even if not implemented.
  friend bool operator==(Value const& a, Value const& b);
  friend bool operator!=(Value const& a, Value const& b) { return !(a == b); }

  /**
   * Outputs string representation of a given Value to the provided stream.
   *
   * @warning This is intended for debugging and human consumption only, not
   *     machine consumption, as the output format may change without notice.
   *
   * @par Example:
   * @code
   *   bigtable::Value v{42};
   *   std::cout << v << "\n";
   * @endcode
   */
  friend std::ostream& operator<<(std::ostream& os, Value const& v);

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

  // Metafunction that returns true if `K` is std::string, Bytes, or int64
  template <typename K>
  struct IsValidMapKey
      : std::integral_constant<
            bool, std::is_same<std::decay_t<K>, std::string>::value ||
                      std::is_same<std::decay_t<K>, Bytes>::value ||
                      std::is_same<std::decay_t<K>, std::int64_t>::value> {};

  // Tag-dispatch overloads to check if a C++ type matches the type specified
  // by the given `Type` proto.
  static bool TypeProtoIs(bool, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(std::int64_t, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(float, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(double, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(std::string const&,
                          google::bigtable::v2::Type const&);
  static bool TypeProtoIs(Bytes const&, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(Timestamp const&, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(absl::CivilDay, google::bigtable::v2::Type const&);
  template <typename T>
  static bool TypeProtoIs(absl::optional<T>,
                          google::bigtable::v2::Type const& type) {
    return TypeProtoIs(T{}, type);
  }
  template <typename T>
  static bool TypeProtoIs(std::vector<T> const&,
                          google::bigtable::v2::Type const& type) {
    return type.has_array_type() &&
           TypeProtoIs(T{}, type.array_type().element_type());
  }
  template <typename... Ts>
  static bool TypeProtoIs(std::tuple<Ts...> const& tup,
                          google::bigtable::v2::Type const& type) {
    bool ok = type.has_struct_type();
    ok = ok && type.struct_type().fields().size() == sizeof...(Ts);
    bigtable_internal::ForEach(tup, IsStructTypeProto{ok, 0},
                               type.struct_type());
    return ok;
  }
  template <typename K, typename V>
  static bool TypeProtoIs(std::unordered_map<K, V> const&,
                          google::bigtable::v2::Type const& type) {
    if (!type.has_map_type()) return false;
    if (!IsValidMapKey<K>()) return false;
    return TypeProtoIs(K{}, type.map_type().key_type()) &&
           TypeProtoIs(V{}, type.map_type().value_type());
  }

  // A functor to be used with internal::ForEach to check if a Type_Struct proto
  // matches the types in a std::tuple.
  struct IsStructTypeProto {
    bool& ok;
    int field;
    template <typename T>
    void operator()(T const&, google::bigtable::v2::Type_Struct const& type) {
      ok = ok && TypeProtoIs(T{}, type.fields(field).type());
      ++field;
    }
    template <typename T>
    void operator()(std::pair<std::string, T> const&,
                    google::bigtable::v2::Type_Struct const& type) {
      operator()(T{}, type);
    }
  };

  // Tag-dispatch overloads to convert a C++ type to a `Type` protobuf. The
  // argument type is the tag, the argument value is ignored.
  static google::bigtable::v2::Type MakeTypeProto(bool);
  static google::bigtable::v2::Type MakeTypeProto(std::int64_t);
  static google::bigtable::v2::Type MakeTypeProto(float);
  static google::bigtable::v2::Type MakeTypeProto(double);
  static google::bigtable::v2::Type MakeTypeProto(std::string const&);
  static google::bigtable::v2::Type MakeTypeProto(Bytes const&);
  static google::bigtable::v2::Type MakeTypeProto(Timestamp const&);
  static google::bigtable::v2::Type MakeTypeProto(absl::CivilDay const&);
  static google::bigtable::v2::Type MakeTypeProto(int);
  static google::bigtable::v2::Type MakeTypeProto(char const*);
  template <typename T>
  static google::bigtable::v2::Type MakeTypeProto(absl::optional<T> const&) {
    return MakeTypeProto(T{});
  }
  template <typename T>
  static google::bigtable::v2::Type MakeTypeProto(std::vector<T> const& v) {
    google::bigtable::v2::Type t;
    t.set_allocated_array_type(
        std::move(new google::bigtable::v2::Type_Array()));
    *t.mutable_array_type()->mutable_element_type() =
        MakeTypeProto(v.empty() ? T{} : v[0]);
    // Checks that vector elements have exactly the same proto Type, which
    // includes field names. This is documented UB.
    for (auto&& e : v) {
      google::bigtable::v2::Type vt = MakeTypeProto(e);
      if (t.array_type().element_type().kind_case() != vt.kind_case())
        internal::ThrowInvalidArgument("Mismatched types");
    }
    return t;
  }
  template <typename... Ts>
  static google::bigtable::v2::Type MakeTypeProto(
      std::tuple<Ts...> const& tup) {
    google::bigtable::v2::Type t;
    t.set_allocated_struct_type(
        std::move(new google::bigtable::v2::Type_Struct()));
    bigtable_internal::ForEach(tup, AddStructTypes{}, *t.mutable_struct_type());
    return t;
  }
  template <typename K, typename V>
  static google::bigtable::v2::Type MakeTypeProto(
      std::unordered_map<K, V> const&) {
    google::bigtable::v2::Type t;
    t.set_allocated_map_type(std::move(new google::bigtable::v2::Type_Map()));
    *t.mutable_map_type()->mutable_key_type() = MakeTypeProto(K{});
    *t.mutable_map_type()->mutable_value_type() = MakeTypeProto(V{});
    return t;
  }

  // A functor to be used with internal::ForEach to add type protos for all the
  // elements of a tuple.
  struct AddStructTypes {
    template <typename T>
    void operator()(T const& t,
                    google::bigtable::v2::Type_Struct& struct_type) const {
      auto* field = struct_type.add_fields();
      *field->mutable_type() = MakeTypeProto(t);
    }
    template <
        typename S, typename T,
        std::enable_if_t<std::is_convertible<S, std::string>::value, int> = 0>
    void operator()(std::pair<S, T> const& p,
                    google::bigtable::v2::Type_Struct& struct_type) const {
      auto* field = struct_type.add_fields();
      field->set_allocated_field_name(std::move(new std::string(p.first)));
      *field->mutable_type() = MakeTypeProto(p.second);
    }
  };

  // Encodes the argument as a protobuf according to the rules described in
  // https://github.com/googleapis/googleapis/blob/master/google/bigtable/v2/type.proto
  static google::bigtable::v2::Value MakeValueProto(bool b);
  static google::bigtable::v2::Value MakeValueProto(std::int64_t i);
  static google::bigtable::v2::Value MakeValueProto(float f);
  static google::bigtable::v2::Value MakeValueProto(double d);
  static google::bigtable::v2::Value MakeValueProto(std::string s);
  static google::bigtable::v2::Value MakeValueProto(Bytes const& b);
  static google::bigtable::v2::Value MakeValueProto(Timestamp const& t);
  static google::bigtable::v2::Value MakeValueProto(absl::CivilDay const& d);
  static google::bigtable::v2::Value MakeValueProto(int i);
  static google::bigtable::v2::Value MakeValueProto(char const* s);
  template <typename T>
  static google::bigtable::v2::Value MakeValueProto(absl::optional<T> opt) {
    if (opt.has_value()) return MakeValueProto(*std::move(opt));
    google::bigtable::v2::Value v;
    v.clear_kind();
    v.clear_type();
    return v;
  }
  template <typename T>
  static google::bigtable::v2::Value MakeValueProto(std::vector<T> vec) {
    google::bigtable::v2::Value v;
    auto& list = *v.mutable_array_value();
    for (auto&& e : vec) {
      *list.add_values() = MakeValueProto(std::move(e));
    }
    return v;
  }
  template <typename... Ts>
  static google::bigtable::v2::Value MakeValueProto(std::tuple<Ts...> tup) {
    google::bigtable::v2::Value v;
    bigtable_internal::ForEach(tup, AddStructValues{},
                               *v.mutable_array_value());
    return v;
  }
  template <typename K, typename V>
  static google::bigtable::v2::Value MakeValueProto(
      std::unordered_map<K, V> m) {
    google::bigtable::v2::Value v;
    auto& list = *v.mutable_array_value();
    for (auto&& kv : m) {
      // we add a subarray for each key-value pair, where the first element
      // is the key and the second element is the value
      google::bigtable::v2::Value item;
      *(*item.mutable_array_value()).add_values() =
          MakeValueProto(std::move(kv.first));
      *(*item.mutable_array_value()).add_values() =
          MakeValueProto(std::move(kv.second));
      *list.add_values() = std::move(item);
    }
    return v;
  }

  // A functor to be used with internal::ForEach to add Value protos for all
  // the elements of a tuple.
  struct AddStructValues {
    template <typename T>
    void operator()(T& t, google::bigtable::v2::ArrayValue& list_value) const {
      *list_value.add_values() = MakeValueProto(std::move(t));
    }
    template <
        typename S, typename T,
        std::enable_if_t<std::is_convertible<S, std::string>::value, int> = 0>
    void operator()(std::pair<S, T> p,
                    google::bigtable::v2::ArrayValue& list_value) const {
      *list_value.add_values() = MakeValueProto(std::move(p.second));
    }
  };

  // Tag-dispatch overloads to extract a C++ value from a `Value` protobuf. The
  // first argument type is the tag, its value is ignored.
  static StatusOr<bool> GetValue(bool, google::bigtable::v2::Value const&,
                                 google::bigtable::v2::Type const&);
  static StatusOr<std::int64_t> GetValue(std::int64_t,
                                         google::bigtable::v2::Value const&,
                                         google::bigtable::v2::Type const&);
  static StatusOr<float> GetValue(float, google::bigtable::v2::Value const&,
                                  google::bigtable::v2::Type const&);
  static StatusOr<double> GetValue(double, google::bigtable::v2::Value const&,
                                   google::bigtable::v2::Type const&);
  static StatusOr<std::string> GetValue(std::string const&,
                                        google::bigtable::v2::Value const&,
                                        google::bigtable::v2::Type const&);
  static StatusOr<std::string> GetValue(std::string const&,
                                        google::bigtable::v2::Value&&,
                                        google::bigtable::v2::Type const&);
  static StatusOr<Bytes> GetValue(Bytes const&,
                                  google::bigtable::v2::Value const&,
                                  google::bigtable::v2::Type const&);
  static StatusOr<Timestamp> GetValue(Timestamp const&,
                                      google::bigtable::v2::Value const&,
                                      google::bigtable::v2::Type const&);
  static StatusOr<absl::CivilDay> GetValue(absl::CivilDay const&,
                                           google::bigtable::v2::Value const&,
                                           google::bigtable::v2::Type const&);

  template <typename T, typename PV>
  static StatusOr<absl::optional<T>> GetValue(
      absl::optional<T> const&, PV&& pv, google::bigtable::v2::Type const& pt) {
    if (pv.kind_case() == google::bigtable::v2::Value::KIND_NOT_SET) {
      return absl::optional<T>{};
    }
    auto value = GetValue(T{}, std::forward<PV>(pv), pt);
    if (!value) return std::move(value).status();
    return absl::optional<T>{*std::move(value)};
  }
  template <typename T, typename PV>
  static StatusOr<std::vector<T>> GetValue(
      std::vector<T> const&, PV&& pv, google::bigtable::v2::Type const& pt) {
    if (!pt.has_array_type() || !pv.has_array_value()) {
      return internal::UnknownError("missing ARRAY", GCP_ERROR_INFO());
    }
    std::vector<T> v;
    for (int i = 0; i < pv.array_value().values().size(); ++i) {
      auto&& e = GetProtoValueArrayElement(std::forward<PV>(pv), i);
      using ET = decltype(e);
      auto value =
          GetValue(T{}, std::forward<ET>(e), pt.array_type().element_type());
      if (!value) return std::move(value).status();
      v.push_back(*std::move(value));
    }
    return v;
  }
  template <typename PV, typename... Ts>
  static StatusOr<std::tuple<Ts...>> GetValue(
      std::tuple<Ts...> const&, PV&& pv, google::bigtable::v2::Type const& pt) {
    if (!pt.has_struct_type() || !pv.has_array_value()) {
      return internal::UnknownError("missing STRUCT", GCP_ERROR_INFO());
    }
    std::tuple<Ts...> tup;
    Status status;  // OK
    ExtractTupleValues<PV> f{status, 0, std::forward<PV>(pv), pt};
    bigtable_internal::ForEach(tup, f);
    if (!status.ok()) return status;
    return tup;
  }
  template <typename K, typename V, typename PV>
  static StatusOr<std::unordered_map<K, V>> GetValue(
      std::unordered_map<K, V> const&, PV&& pv,
      google::bigtable::v2::Type const& pt) {
    if (!pt.has_map_type() || !pv.has_array_value()) {
      return internal::UnknownError("missing MAP", GCP_ERROR_INFO());
    }
    std::unordered_map<K, V> m;
    for (int i = 0; i < pv.array_value().values().size(); ++i) {
      auto&& map_value_proto =
          GetProtoValueArrayElement(std::forward<PV>(pv), i);
      using ET = decltype(map_value_proto);
      // map key-value pairs are assumed to be an array of size 2
      if (!map_value_proto.has_array_value() ||
          map_value_proto.array_value().values().size() != 2) {
        return internal::UnknownError("malformed key-value pair",
                                      GCP_ERROR_INFO());
      }
      auto&& key_proto =
          GetProtoValueArrayElement(std::forward<ET>(map_value_proto), 0);
      auto&& value_proto =
          GetProtoValueArrayElement(std::forward<ET>(map_value_proto), 1);
      using KeyProto = decltype(key_proto);
      using ValueProto = decltype(value_proto);
      auto const& key = GetValue(K{}, std::forward<KeyProto>(key_proto),
                                 pt.map_type().key_type());
      auto const& value = GetValue(V{}, std::forward<ValueProto>(value_proto),
                                   pt.map_type().value_type());
      if (!key) return std::move(key).status();
      if (!value) return std::move(value).status();

      // the documented behavior indicates that the last value will take
      // precedence for a given key.
      auto const& pos = m.find(key.value());
      if (pos != m.end()) {
        m.erase(pos);
      }
      m.insert(std::make_pair(*std::move(key), *std::move(value)));
    }
    return m;
  }

  // A functor to be used with internal::ForEach to extract C++ types from a
  // bigtable::v2::Value proto and with array value store then in a tuple.
  template <typename V>
  struct ExtractTupleValues {
    Status& status;
    int i;
    V&& pv;
    google::bigtable::v2::Type const& type;
    template <typename T>
    void operator()(T& t) {
      auto&& e = GetProtoValueArrayElement(std::forward<V>(pv), i);
      auto et = type.struct_type().fields(i).type();
      using ET = decltype(e);
      auto value = GetValue(T{}, std::forward<ET>(e), et);
      ++i;
      if (!value) {
        status = std::move(value).status();
      } else {
        t = *std::move(value);
      }
    }
    template <typename T>
    void operator()(std::pair<std::string, T>& p) {
      p.first = type.struct_type().fields(i).field_name();
      auto&& e = GetProtoValueArrayElement(std::forward<V>(pv), i);
      auto et = type.struct_type().fields(i).type();
      using ET = decltype(e);
      auto value = GetValue(T{}, std::forward<ET>(e), et);
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
  // functions. To make GetValue(vector<T>, ...) or GetValue(tuple<K,V>, ...)
  // (above) work, we need split the different protobuf syntaxes into
  // overloaded functions.
  static google::bigtable::v2::Value const& GetProtoValueArrayElement(
      google::bigtable::v2::Value const& pv, int pos) {
    return pv.array_value().values(pos);
  }
  static google::bigtable::v2::Value&& GetProtoValueArrayElement(
      google::bigtable::v2::Value&& pv, int pos) {
    return std::move(*pv.mutable_array_value()->mutable_values(pos));
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

  Value(google::bigtable::v2::Type t, google::bigtable::v2::Value v)
      : type_(std::move(t)), value_(std::move(v)) {}

  friend struct bigtable_internal::ValueInternals;

  google::bigtable::v2::Type type_;
  google::bigtable::v2::Value value_;
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ValueInternals {
  static bigtable::Value FromProto(google::bigtable::v2::Type t,
                                   google::bigtable::v2::Value v) {
    return bigtable::Value(std::move(t), std::move(v));
  }

  static std::pair<google::bigtable::v2::Type, google::bigtable::v2::Value>
  ToProto(bigtable::Value v) {
    return std::make_pair(std::move(v.type_), std::move(v.value_));
  }
};

inline bigtable::Value FromProto(google::bigtable::v2::Type t,
                                 google::bigtable::v2::Value v) {
  return ValueInternals::FromProto(std::move(t), std::move(v));
}

inline std::pair<google::bigtable::v2::Type, google::bigtable::v2::Value>
ToProto(bigtable::Value v) {
  return ValueInternals::ToProto(std::move(v));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_VALUE_H
