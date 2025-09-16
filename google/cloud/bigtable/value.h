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

#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"
#include <google/bigtable/v2/data.pb.h>
#include <google/bigtable/v2/types.pb.h>

namespace google {
namespace cloud {

namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
struct ValueInternals;

static std::string const INVALID_FLOAT_VALUE_MESSAGE =
    "NaN and Infinity are not supported for FLOAT** values";

static bool _validate_float_value(double v) {
  std::cout << v << std::endl;
  if (std::isnan(v) || std::isinf(v)) {
    throw internal::FailedPreconditionError(INVALID_FLOAT_VALUE_MESSAGE);
  }
  return true;
}

static bool ValidateFloatValue(double v) { return _validate_float_value(v); }

static bool ValidateFloatValue(float v) { return _validate_float_value(v); }
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
 *
 * Callers may create instances by passing any of the supported values
 * (shown in the table above) to the constructor. "Null" values are created
 * using the `MakeNullValue<T>()` factory function or by passing an empty
 * `absl::optional<T>` to the Value constructor.
 *
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
  /// @copydoc Value(bool)
  explicit Value(std::string v) : Value(PrivateConstructor{}, std::move(v)) {}
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

  // Tag-dispatch overloads to check if a C++ type matches the type specified
  // by the given `Type` proto.
  static bool TypeProtoIs(bool, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(std::int64_t, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(float, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(double, google::bigtable::v2::Type const&);
  static bool TypeProtoIs(std::string const&,
                          google::bigtable::v2::Type const&);
  template <typename T>
  static bool TypeProtoIs(absl::optional<T>,
                          google::bigtable::v2::Type const& type) {
    return TypeProtoIs(T{}, type);
  }

  // Tag-dispatch overloads to convert a C++ type to a `Type` protobuf. The
  // argument type is the tag, the argument value is ignored.
  static google::bigtable::v2::Type MakeTypeProto(bool);
  static google::bigtable::v2::Type MakeTypeProto(std::int64_t);
  static google::bigtable::v2::Type MakeTypeProto(float);
  static google::bigtable::v2::Type MakeTypeProto(double);
  static google::bigtable::v2::Type MakeTypeProto(std::string const&);
  static google::bigtable::v2::Type MakeTypeProto(int);
  static google::bigtable::v2::Type MakeTypeProto(char const*);
  template <typename T>
  static google::bigtable::v2::Type MakeTypeProto(absl::optional<T> const&) {
    return MakeTypeProto(T{});
  }

  // Encodes the argument as a protobuf according to the rules described in
  // https://github.com/googleapis/googleapis/blob/master/google/bigtable/v2/type.proto
  static google::bigtable::v2::Value MakeValueProto(bool b);
  static google::bigtable::v2::Value MakeValueProto(std::int64_t i);
  static google::bigtable::v2::Value MakeValueProto(float f);
  static google::bigtable::v2::Value MakeValueProto(double d);
  static google::bigtable::v2::Value MakeValueProto(std::string s);
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

  template <typename T, typename V>
  static StatusOr<absl::optional<T>> GetValue(
      absl::optional<T> const&, V&& pv, google::bigtable::v2::Type const& pt) {
    if (pv.kind_case() == google::bigtable::v2::Value::KIND_NOT_SET) {
      return absl::optional<T>{};
    }
    auto value = GetValue(T{}, std::forward<V>(pv), pt);
    if (!value) return std::move(value).status();
    return absl::optional<T>{*std::move(value)};
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
