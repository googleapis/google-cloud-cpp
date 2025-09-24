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

#include "google/cloud/bigtable/value.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/strings/cord.h"
#include <google/bigtable/v2/types.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Some Bigtable proto fields use Cord internally and string externally.
template <typename T, typename std::enable_if<
                          std::is_same<T, std::string>::value>::type* = nullptr>
std::string AsString(T const& s) {
  return s;
}
template <typename T, typename std::enable_if<
                          std::is_same<T, std::string>::value>::type* = nullptr>
std::string AsString(T&& s) {
  return std::move(s);  // NOLINT(bugprone-move-forwarding-reference)
}
template <typename T, typename std::enable_if<
                          std::is_same<T, absl::Cord>::value>::type* = nullptr>
std::string AsString(T const& s) {
  return std::string(s);
}
template <typename T, typename std::enable_if<
                          std::is_same<T, absl::Cord>::value>::type* = nullptr>
std::string AsString(T&& s) {
  return std::string(
      std::move(s));  // NOLINT(bugprone-move-forwarding-reference)
}

// Compares two sets of Type and Value protos for equality. This method calls
// itself recursively to compare subtypes and subvalues.
bool Equal(google::bigtable::v2::Type const& pt1,  // NOLINT(misc-no-recursion)
           google::bigtable::v2::Value const& pv1,
           google::bigtable::v2::Type const& pt2,
           google::bigtable::v2::Value const& pv2) {
  if (pt1.kind_case() != pt2.kind_case()) return false;
  if (pv1.kind_case() != pv2.kind_case()) return false;
  if (pt1.has_bool_type()) {
    return pv1.bool_value() == pv2.bool_value();
  }
  if (pt1.has_int64_type()) {
    return pv1.int_value() == pv2.int_value();
  }
  if (pt1.has_float32_type() || pt1.has_float64_type()) {
    return pv1.float_value() == pv2.float_value();
  }
  if (pt1.has_string_type()) {
    return pv1.string_value() == pv2.string_value();
  }
  if (pt1.has_bytes_type()) {
    return pv1.bytes_value() == pv2.bytes_value();
  }
  return false;
}

// From the proto description, `NULL` values are represented by having a kind
// equal to KIND_NOT_SET
bool IsNullValue(google::bigtable::v2::Value const& value) {
  return value.kind_case() == google::bigtable::v2::Value::KIND_NOT_SET;
}

// An enum to tell StreamHelper() whether a value is being printed as a scalar
// or as part of an aggregate type (i.e., a vector or tuple). Some types may
// format themselves differently in each case.
enum class StreamMode { kScalar, kAggregate };

std::ostream& StreamHelper(std::ostream& os,  // NOLINT(misc-no-recursion)
                           google::bigtable::v2::Value const& v,
                           google::bigtable::v2::Type const&, StreamMode) {
  if (IsNullValue(v)) {
    return os << "NULL";
  }

  if (v.kind_case() == google::bigtable::v2::Value::kBoolValue) {
    return os << v.bool_value();
  }
  if (v.kind_case() == google::bigtable::v2::Value::kIntValue) {
    return os << v.int_value();
  }
  if (v.kind_case() == google::bigtable::v2::Value::kFloatValue) {
    return os << v.float_value();
  }
  if (v.kind_case() == google::bigtable::v2::Value::kStringValue) {
    return os << v.string_value();
  }
  if (v.kind_case() == google::bigtable::v2::Value::kBytesValue) {
    return os << Bytes(AsString(v.bytes_value()));
  }
  // this should include type name
  return os << "Error: unknown value type code ";
}
}  // namespace

bool operator==(Value const& a, Value const& b) {
  return Equal(a.type_, a.value_, b.type_, b.value_);
}

std::ostream& operator<<(std::ostream& os, Value const& v) {
  return StreamHelper(os, v.value_, v.type_, StreamMode::kScalar);
}

//
// Value::TypeProtoIs
//

bool Value::TypeProtoIs(bool, google::bigtable::v2::Type const& type) {
  return type.has_bool_type();
}
bool Value::TypeProtoIs(std::int64_t, google::bigtable::v2::Type const& type) {
  return type.has_int64_type();
}
bool Value::TypeProtoIs(float, google::bigtable::v2::Type const& type) {
  return type.has_float32_type();
}
bool Value::TypeProtoIs(double, google::bigtable::v2::Type const& type) {
  return type.has_float64_type();
}
bool Value::TypeProtoIs(std::string const&,
                        google::bigtable::v2::Type const& type) {
  return type.has_string_type();
}
bool Value::TypeProtoIs(Bytes const&, google::bigtable::v2::Type const& type) {
  return type.has_bytes_type();
}

//
// Value::MakeTypeProto
//

google::bigtable::v2::Type Value::MakeTypeProto(bool) {
  google::bigtable::v2::Type t;
  t.set_allocated_bool_type(std::move(new google::bigtable::v2::Type_Bool()));
  return t;
}
google::bigtable::v2::Type Value::MakeTypeProto(int const i) {
  return Value::MakeTypeProto(static_cast<std::int64_t>(i));
}
google::bigtable::v2::Type Value::MakeTypeProto(std::int64_t) {
  google::bigtable::v2::Type t;
  t.set_allocated_int64_type(std::move(new google::bigtable::v2::Type_Int64()));
  return t;
}
google::bigtable::v2::Type Value::MakeTypeProto(float) {
  google::bigtable::v2::Type t;
  t.set_allocated_float32_type(
      std::move(new google::bigtable::v2::Type_Float32()));
  return t;
}
google::bigtable::v2::Type Value::MakeTypeProto(double) {
  google::bigtable::v2::Type t;
  t.set_allocated_float64_type(
      std::move(new google::bigtable::v2::Type_Float64()));
  return t;
}
google::bigtable::v2::Type Value::MakeTypeProto(std::string const&) {
  google::bigtable::v2::Type t;
  t.set_allocated_string_type(
      std::move(new google::bigtable::v2::Type_String()));
  return t;
}
google::bigtable::v2::Type Value::MakeTypeProto(char const* s) {
  return Value::MakeTypeProto(std::string(std::move(s)));
}
google::bigtable::v2::Type Value::MakeTypeProto(Bytes const&) {
  google::bigtable::v2::Type t;
  t.set_allocated_bytes_type(std::move(new google::bigtable::v2::Type_Bytes()));
  return t;
}

//
// Value::MakeValueProto
//

google::bigtable::v2::Value Value::MakeValueProto(bool b) {
  google::bigtable::v2::Value v;
  v.set_bool_value(b);
  return v;
}
google::bigtable::v2::Value Value::MakeValueProto(std::int64_t i) {
  google::bigtable::v2::Value v;
  v.set_int_value(i);
  return v;
}
google::bigtable::v2::Value Value::MakeValueProto(int i) {
  return Value::MakeValueProto(static_cast<std::int64_t>(i));
}
google::bigtable::v2::Value Value::MakeValueProto(float f) {
  // NaN and Infinity are not supported. See
  // https://github.com/googleapis/googleapis/blob/5caeec4d72173ea3f2772b1b67a5c3f9192a6d06/google/bigtable/v2/data.proto#L140-L142
  if (std::isnan(f) || std::isinf(f)) {
    internal::ThrowInvalidArgument(
        bigtable_internal::kInvalidFloatValueMessage);
  }
  google::bigtable::v2::Value v;
  v.set_float_value(f);
  return v;
}
google::bigtable::v2::Value Value::MakeValueProto(double d) {
  // NaN and Infinity are not supported. See
  // https://github.com/googleapis/googleapis/blob/5caeec4d72173ea3f2772b1b67a5c3f9192a6d06/google/bigtable/v2/data.proto#L140-L142
  if (std::isnan(d) || std::isinf(d)) {
    internal::ThrowInvalidArgument(
        bigtable_internal::kInvalidFloatValueMessage);
  }
  google::bigtable::v2::Value v;
  v.set_float_value(d);
  return v;
}
google::bigtable::v2::Value Value::MakeValueProto(std::string s) {
  google::bigtable::v2::Value v;
  v.set_string_value(std::move(s));
  return v;
}
google::bigtable::v2::Value Value::MakeValueProto(char const* s) {
  return Value::MakeValueProto(std::string(s));
}
google::bigtable::v2::Value Value::MakeValueProto(Bytes const& b) {
  google::bigtable::v2::Value v;
  v.set_bytes_value(b.get<std::string>());
  return v;
}

//
// Value::GetValue
//

StatusOr<bool> Value::GetValue(bool, google::bigtable::v2::Value const& pv,
                               google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kBoolValue) {
    return internal::UnknownError("missing BOOL", GCP_ERROR_INFO());
  }
  return pv.bool_value();
}
StatusOr<std::int64_t> Value::GetValue(std::int64_t,
                                       google::bigtable::v2::Value const& pv,
                                       google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kIntValue) {
    return internal::UnknownError("missing INT64", GCP_ERROR_INFO());
  }
  return pv.int_value();
}
StatusOr<float> Value::GetValue(float, google::bigtable::v2::Value const& pv,
                                google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kFloatValue) {
    return internal::UnknownError("missing FLOAT32", GCP_ERROR_INFO());
  }
  if (std::isnan(pv.float_value()) || std::isinf(pv.float_value())) {
    return internal::UnimplementedError(
        bigtable_internal::kInvalidFloatValueMessage);
  }
  return static_cast<float>(pv.float_value());
}
StatusOr<double> Value::GetValue(double, google::bigtable::v2::Value const& pv,
                                 google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kFloatValue) {
    return internal::UnknownError("missing FLOAT64", GCP_ERROR_INFO());
  }
  if (std::isnan(pv.float_value()) || std::isinf(pv.float_value())) {
    return internal::UnimplementedError(
        bigtable_internal::kInvalidFloatValueMessage);
  }
  return pv.float_value();
}
StatusOr<std::string> Value::GetValue(std::string const&,
                                      google::bigtable::v2::Value const& pv,
                                      google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kStringValue) {
    return internal::UnknownError("missing STRING", GCP_ERROR_INFO());
  }
  return AsString(pv.string_value());
}
StatusOr<std::string> Value::GetValue(std::string const&,
                                      google::bigtable::v2::Value&& pv,
                                      google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kStringValue) {
    return internal::UnknownError("missing STRING", GCP_ERROR_INFO());
  }
  return AsString(std::move(*pv.mutable_string_value()));
}
StatusOr<Bytes> Value::GetValue(Bytes const&,
                                google::bigtable::v2::Value const& pv,
                                google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kBytesValue) {
    return internal::UnknownError("missing BYTES", GCP_ERROR_INFO());
  }
  return Bytes(AsString(pv.bytes_value()));
}

bool Value::is_null() const { return IsNullValue(value_); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
