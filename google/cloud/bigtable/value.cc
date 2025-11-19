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
#include "google/cloud/bigtable/timestamp.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/cord.h"
#include "absl/strings/substitute.h"
#include <google/bigtable/v2/types.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/type/date.pb.h>
#include <algorithm>

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

// Forward declarations for mutually recursive functions.
bool Equal(google::bigtable::v2::Type const& pt1,  // NOLINT(misc-no-recursion)
           google::bigtable::v2::Value const& pv1,
           google::bigtable::v2::Type const& pt2,
           google::bigtable::v2::Value const& pv2);

bool ArrayEqual(  // NOLINT(misc-no-recursion)
    google::bigtable::v2::Type const& pt1,
    google::bigtable::v2::Value const& pv1,
    google::bigtable::v2::Type const& pt2,
    google::bigtable::v2::Value const& pv2);

bool StructEqual(  // NOLINT(misc-no-recursion)
    google::bigtable::v2::Type const& pt1,
    google::bigtable::v2::Value const& pv1,
    google::bigtable::v2::Type const& pt2,
    google::bigtable::v2::Value const& pv2);

bool MapEqual(  // NOLINT(misc-no-recursion)
    google::bigtable::v2::Type const& pt1,
    google::bigtable::v2::Value const& pv1,
    google::bigtable::v2::Type const& pt2,
    google::bigtable::v2::Value const& pv2);

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
  if (pt1.has_timestamp_type()) {
    return pv1.timestamp_value().seconds() == pv2.timestamp_value().seconds() &&
           pv1.timestamp_value().nanos() == pv2.timestamp_value().nanos();
  }
  if (pt1.has_date_type()) {
    return pv1.date_value().day() == pv2.date_value().day() &&
           pv1.date_value().month() == pv2.date_value().month() &&
           pv1.date_value().year() == pv2.date_value().year();
  }
  if (pt1.has_array_type()) {
    return ArrayEqual(pt1, pv1, pt2, pv2);
  }
  if (pt1.has_struct_type()) {
    return StructEqual(pt1, pv1, pt2, pv2);
  }
  if (pt1.has_map_type()) {
    return MapEqual(pt1, pv1, pt2, pv2);
  }
  return false;
}

// Compares two sets of Type and Value protos that represent an ARRAY for
// equality.
bool ArrayEqual(  // NOLINT(misc-no-recursion)
    google::bigtable::v2::Type const& pt1,
    google::bigtable::v2::Value const& pv1,
    google::bigtable::v2::Type const& pt2,
    google::bigtable::v2::Value const& pv2) {
  auto const& vec1 = pv1.array_value().values();
  auto const& vec2 = pv2.array_value().values();
  if (vec1.size() != vec2.size()) {
    return false;
  }
  auto const& el_type1 = pt1.array_type().element_type();
  auto const& el_type2 = pt2.array_type().element_type();
  if (el_type1.kind_case() != el_type2.kind_case()) {
    return false;
  }
  for (int i = 0; i < vec1.size(); ++i) {
    if (!Equal(el_type1, vec1.Get(i), el_type2, vec2.Get(i))) {
      return false;
    }
  }
  return true;
}

// Compares two sets of Type and Value protos that represent a STRUCT for
// equality.
bool StructEqual(  // NOLINT(misc-no-recursion)
    google::bigtable::v2::Type const& pt1,
    google::bigtable::v2::Value const& pv1,
    google::bigtable::v2::Type const& pt2,
    google::bigtable::v2::Value const& pv2) {
  auto const& fields1 = pt1.struct_type().fields();
  auto const& fields2 = pt2.struct_type().fields();
  if (fields1.size() != fields2.size()) return false;
  auto const& v1 = pv1.array_value().values();
  auto const& v2 = pv2.array_value().values();
  if (fields1.size() != v1.size() || v1.size() != v2.size()) return false;
  for (int i = 0; i < fields1.size(); ++i) {
    auto const& f1 = fields1.Get(i);
    auto const& f2 = fields2.Get(i);
    if (f1.field_name() != f2.field_name()) return false;
    if (!Equal(f1.type(), v1.Get(i), f2.type(), v2.Get(i))) {
      return false;
    }
  }
  return true;
}

// Compares two sets of Type and Value protos that represent a MAP for
// equality.
bool MapEqual(  // NOLINT(misc-no-recursion)
    google::bigtable::v2::Type const& pt1,
    google::bigtable::v2::Value const& pv1,
    google::bigtable::v2::Type const& pt2,
    google::bigtable::v2::Value const& pv2) {
  auto const& kt1 = pt1.map_type().key_type();
  auto const& kt2 = pt2.map_type().key_type();
  auto const& vt1 = pt1.map_type().value_type();
  auto const& vt2 = pt2.map_type().value_type();
  if (kt1.kind_case() != kt2.kind_case()) return false;
  if (vt1.kind_case() != vt2.kind_case()) return false;

  auto const& mv1 = pv1.array_value().values();
  auto const& mv2 = pv2.array_value().values();
  if (mv1.size() != mv2.size()) return false;
  // We double-check that all subarrays are of size 2;
  for (int i = 0; i < mv1.size(); ++i) {
    auto const& f1 = mv1.Get(i);
    auto const& f2 = mv2.Get(i);
    if (f1.array_value().values_size() != 2) return false;
    if (f2.array_value().values_size() != 2) return false;
  }
  // NOLINTNEXTLINE(misc-no-recursion)
  auto comparison_function = [&kt1, &kt2, &vt1, &vt2](
                                 google::bigtable::v2::Value const& f1,
                                 google::bigtable::v2::Value const& f2) {
    auto const& k1 = f1.array_value().values(0);
    auto const& k2 = f2.array_value().values(0);
    auto const& v1 = f1.array_value().values(1);
    auto const& v2 = f2.array_value().values(1);
    return Equal(kt1, k1, kt2, k2) && Equal(vt1, v1, vt2, v2);
  };
  return std::is_permutation(mv1.begin(), mv1.end(), mv2.begin(), mv2.end(),
                             comparison_function);
}

// A helper to escape all double quotes in the given string `s`. For example,
// if given `"foo"`, outputs `\"foo\"`. This is useful when a caller needs to
// wrap `s` itself in double quotes.
std::ostream& EscapeQuotes(std::ostream& os, std::string const& s) {
  for (auto const& c : s) {
    if (c == '"') os << "\\";
    os << c;
  }
  return os;
}

// An enum to tell StreamHelper() whether a value is being printed as a scalar
// or as part of an aggregate type (i.e., a vector or tuple). Some types may
// format themselves differently in each case.
enum class StreamMode { kScalar, kAggregate };

std::ostream& MapStreamHelper(std::ostream& os,  // NOLINT(misc-no-recursion)
                              google::bigtable::v2::Value const& v,
                              google::bigtable::v2::Type const& t,
                              StreamMode mode);

std::ostream& StreamHelper(std::ostream& os,  // NOLINT(misc-no-recursion)
                           google::bigtable::v2::Value const& v,
                           google::bigtable::v2::Type const& t,
                           StreamMode mode) {
  if (Value::IsNullValue(v)) {
    return os << "NULL";
  }

  if (t.has_bool_type()) {
    return os << v.bool_value();
  }
  if (t.has_int64_type()) {
    return os << v.int_value();
  }
  if (t.has_float32_type() || t.has_float64_type()) {
    return os << v.float_value();
  }
  if (t.has_string_type()) {
    switch (mode) {
      case StreamMode::kScalar:
        return os << v.string_value();
      case StreamMode::kAggregate:
        os << '"';
        EscapeQuotes(os, AsString(v.string_value()));
        return os << '"';
    }
    return os;  // Unreachable, but quiets warning.
  }
  if (t.has_bytes_type()) {
    return os << Bytes(AsString(v.bytes_value()));
  }
  if (t.has_timestamp_type()) {
    auto ts = MakeTimestamp(v.timestamp_value());
    if (!ts) {
      internal::ThrowStatus(ts.status());
    }
    return os << ts.value();
  }
  if (t.has_date_type()) {
    auto date =
        bigtable_internal::FromProto(t, v).get<absl::CivilDay>().value();
    return os << date;
  }
  if (t.has_array_type()) {
    auto const* delimiter = "";
    os << '[';
    for (auto&& val : v.array_value().values()) {
      os << delimiter;
      StreamHelper(os, val, t.array_type().element_type(),
                   StreamMode::kAggregate);
      delimiter = ", ";
    }
    return os << ']';
  }
  if (t.has_struct_type()) {
    auto const* delimiter = "";
    os << '(';
    for (int i = 0; i < v.array_value().values_size(); ++i) {
      os << delimiter;
      if (!t.struct_type().fields(i).field_name().empty()) {
        os << '"';
        EscapeQuotes(os, t.struct_type().fields(i).field_name());
        os << '"' << ": ";
      }
      StreamHelper(os, v.array_value().values(i),
                   t.struct_type().fields(i).type(), StreamMode::kAggregate);
      delimiter = ", ";
    }
    return os << ')';
  }
  if (t.has_map_type()) {
    return MapStreamHelper(os, v, t, mode);
  }
  // this should include type name
  return os << "Error: unknown value type code " << t.kind_case();
}
std::ostream& MapStreamHelper(std::ostream& os,  // NOLINT(misc-no-recursion)
                              google::bigtable::v2::Value const& v,
                              google::bigtable::v2::Type const& t, StreamMode) {
  auto const* delimiter = "";
  os << '{';
  for (int i = 0; i < v.array_value().values_size(); ++i) {
    os << delimiter;
    os << "{";
    auto const& kv = v.array_value().values(i);
    if (!kv.has_array_value() || kv.array_value().values_size() != 2) {
      os << "malformed key-value pair";
      delimiter = ", ";
      continue;
    }
    StreamHelper(os, kv.array_value().values(0), t.map_type().key_type(),
                 StreamMode::kAggregate);
    os << " : ";
    StreamHelper(os, kv.array_value().values(1), t.map_type().value_type(),
                 StreamMode::kAggregate);
    os << "}";
    delimiter = ", ";
  }
  return os << '}';
}
}  // namespace

bool operator==(Value const& a, Value const& b) {
  return bigtable::Equal(a.type_, a.value_, b.type_, b.value_);
}

std::ostream& operator<<(std::ostream& os, Value const& v) {
  return StreamHelper(os, v.value_, v.type_, StreamMode::kScalar);
}

Status MakeDepthExceededError() {
  return internal::InternalError("Nested value depth exceeds 10 levels",
                                 GCP_ERROR_INFO());
}

// NOLINTNEXTLINE(misc-no-recursion)
Status TypeAndArrayValuesMatch(google::bigtable::v2::Type const& type,
                               google::bigtable::v2::Value const& value,
                               int depth) {
  if (depth > 10) {
    return MakeDepthExceededError();
  }
  if (!value.has_array_value()) {
    return internal::InternalError(
        "Value kind must be ARRAY_VALUE for columns of type: MAP");
  }
  auto const& vals = value.array_value().values();
  for (auto const& val : vals) {
    auto const element_match_result =
        Value::TypeAndValuesMatch(type.array_type().element_type(), val, depth);
    if (!element_match_result.ok()) {
      return element_match_result;
    }
  }
  return Status{};
}

// NOLINTNEXTLINE(misc-no-recursion)
Status TypeAndMapValuesMatch(google::bigtable::v2::Type const& type,
                             google::bigtable::v2::Value const& value,
                             int depth) {
  if (depth > 10) {
    return MakeDepthExceededError();
  }
  if (!value.has_array_value()) {
    return internal::InternalError(
        "Value kind must be ARRAY_VALUE for columns of type: MAP");
  }
  auto key_type = type.map_type().key_type();
  auto value_type = type.map_type().value_type();
  auto const& vals = value.array_value().values();
  for (auto const& val : vals) {
    if (!val.has_array_value() || val.array_value().values_size() != 2) {
      return internal::InternalError(
          "ARRAY_VALUE must contain entries of 2 values");
    }
    auto map_key = val.array_value().values(0);
    auto map_value = val.array_value().values(1);
    // NOLINTNEXTLINE(misc-no-recursion)
    auto key_match_result =
        Value::TypeAndValuesMatch(key_type, map_key, ++depth);
    if (!key_match_result.ok()) {
      return key_match_result;
    }
    // NOLINTNEXTLINE(misc-no-recursion)
    auto value_match_result =
        Value::TypeAndValuesMatch(value_type, map_value, ++depth);
    if (!value_match_result.ok()) {
      return value_match_result;
    }
  }
  return Status{};
}

// NOLINTNEXTLINE(misc-no-recursion)
Status TypeAndStructValuesMatch(google::bigtable::v2::Type const& type,
                                google::bigtable::v2::Value const& value,
                                int depth) {
  if (depth > 10) {
    return MakeDepthExceededError();
  }
  if (!value.has_array_value()) {
    return internal::InternalError(
        "Value kind must be ARRAY_VALUE for columns of type: STRUCT");
  }
  auto fields = type.struct_type().fields();
  auto values = value.array_value().values();
  if (fields.size() != values.size()) {
    auto const message = absl::Substitute(
        "received Struct with $0 values, but metadata has $1 fields",
        values.size(), fields.size());
    return internal::InternalError(message);
  }
  for (int i = 0; i < fields.size(); ++i) {
    auto const& f1 = fields.Get(i);
    auto const& v = values[i];
    auto match_result = Value::TypeAndValuesMatch(f1.type(), v, ++depth);
    if (!match_result.ok()) {
      return match_result;
    }
  }
  return Status{};
}

/**
 * Checks whether the declared type in column matches the value's contents.
 * Since the received values may or may not have type() set, we check against
 * the value contents themselves
 */
// NOLINTNEXTLINE(misc-no-recursion)
Status Value::TypeAndValuesMatch(google::bigtable::v2::Type const& type,
                                 google::bigtable::v2::Value const& value,
                                 int depth) {
  if (depth > 10) {
    return MakeDepthExceededError();
  }
  using google::bigtable::v2::Type;
  auto make_mismatch_metadata_status = [&](std::string const& value_kind,
                                           std::string const& type_name) {
    auto const message = absl::Substitute(
        "Value kind must be $0 for columns of type: $1", value_kind, type_name);
    return internal::InternalError(message);
  };
  // Null values are allowed by default
  if (IsNullValue(value)) {
    return Status{};
  }
  Status result;
  switch (type.kind_case()) {
    case Type::kArrayType:
      result = TypeAndArrayValuesMatch(type, value, ++depth);
      break;
    case Type::kMapType:
      result = TypeAndMapValuesMatch(type, value, ++depth);
      break;
    case Type::kStructType:
      result = TypeAndStructValuesMatch(type, value, ++depth);
      break;
    case Type::kBoolType:
      if (!value.has_bool_value()) {
        result = make_mismatch_metadata_status("BOOL_VALUE", "BOOL");
      }
      break;
    case Type::kBytesType:
      if (!value.has_bytes_value()) {
        result = make_mismatch_metadata_status("BYTES_VALUE", "BYTES");
      }
      break;
    case Type::kDateType:
      if (!value.has_date_value()) {
        result = make_mismatch_metadata_status("DATE_VALUE", "DATE");
      }
      break;
    case Type::kFloat32Type:
      if (!value.has_float_value()) {
        result = make_mismatch_metadata_status("FLOAT_VALUE", "FLOAT32");
      }
      break;
    case Type::kFloat64Type:
      if (!value.has_float_value()) {
        result = make_mismatch_metadata_status("FLOAT_VALUE", "FLOAT64");
      }
      break;
    case Type::kInt64Type:
      if (!value.has_int_value()) {
        result = make_mismatch_metadata_status("INT_VALUE", "INT64");
      }
      break;
    case Type::kStringType:
      if (!value.has_string_value()) {
        result = make_mismatch_metadata_status("STRING_VALUE", "STRING");
      }
      break;
    case Type::kTimestampType:
      if (!value.has_timestamp_value()) {
        result = make_mismatch_metadata_status("TIMESTAMP_VALUE", "TIMESTAMP");
      }
      break;
    default:
      result = internal::InternalError("Unsupported type");
      break;
  }
  // Nulls are allowed;
  return result;
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
bool Value::TypeProtoIs(Timestamp const&,
                        google::bigtable::v2::Type const& type) {
  return type.has_timestamp_type();
}
bool Value::TypeProtoIs(absl::CivilDay,
                        google::bigtable::v2::Type const& type) {
  return type.has_date_type();
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
google::bigtable::v2::Type Value::MakeTypeProto(Timestamp const&) {
  google::bigtable::v2::Type t;
  t.set_allocated_timestamp_type(
      std::move(new google::bigtable::v2::Type_Timestamp()));
  return t;
}
google::bigtable::v2::Type Value::MakeTypeProto(absl::CivilDay const&) {
  google::bigtable::v2::Type t;
  t.set_allocated_date_type(std::move(new google::bigtable::v2::Type_Date()));
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
google::bigtable::v2::Value Value::MakeValueProto(Timestamp const& t) {
  google::bigtable::v2::Value v;
  auto proto_ts = t.get<protobuf::Timestamp>();
  if (!proto_ts) {
    internal::ThrowStatus(std::move(proto_ts).status());
  }
  auto proto = std::make_unique<protobuf::Timestamp>(*std::move(proto_ts));
  v.set_allocated_timestamp_value(proto.release());
  return v;
}

google::bigtable::v2::Value Value::MakeValueProto(absl::CivilDay const& d) {
  auto date = type::Date();
  date.set_day(d.day());
  date.set_month(d.month());
  date.set_year(static_cast<int32_t>(d.year()));
  auto proto = std::make_unique<type::Date>(std::move(date));
  google::bigtable::v2::Value v;
  v.set_allocated_date_value(proto.release());
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
StatusOr<Timestamp> Value::GetValue(Timestamp const&,
                                    google::bigtable::v2::Value const& pv,
                                    google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kTimestampValue) {
    return internal::UnknownError("missing TIMESTAMP", GCP_ERROR_INFO());
  }
  return MakeTimestamp(pv.timestamp_value());
}

StatusOr<absl::CivilDay> Value::GetValue(absl::CivilDay const&,
                                         google::bigtable::v2::Value const& pv,
                                         google::bigtable::v2::Type const&) {
  if (pv.kind_case() != google::bigtable::v2::Value::kDateValue) {
    return internal::UnknownError("missing DATE", GCP_ERROR_INFO());
  }
  type::Date const& v = pv.date_value();
  auto date = absl::CivilDay(v.year(), v.month(), v.day());
  return date;
}

bool Value::is_null() const { return IsNullValue(value_); }

// We're calling this function from a constructor which we prefer to always
// complete. Any errors encountered will be deferred, and we will report them on
// attempts at accessing the value.
// Any duplicates keys found in the map will be deduped to use the last value
// specified for the key, per:
// https://github.com/googleapis/googleapis/blob/0eeb1be5b78a9c7e006ee57cde95349834ae9f3b/google/bigtable/v2/types.proto#L357
void Value::DedupProtoMap() {
  google::bigtable::v2::Value dedup_value;
  absl::flat_hash_set<std::string> keys;
  for (int i = value_.array_value().values_size() - 1; i >= 0; --i) {
    auto map_value_proto = GetProtoValueArrayElement(value_, i);
    if (!map_value_proto.has_array_value() ||
        map_value_proto.array_value().values_size() != 2) {
      continue;
    }
    auto const& key = map_value_proto.array_value().values(0);
    // TODO(#15766): Find a better way to hash these protos.
    if (keys.insert(key.SerializeAsString()).second) {
      *dedup_value.mutable_array_value()->add_values() =
          std::move(map_value_proto);
    }
  }
  std::reverse(dedup_value.mutable_array_value()->mutable_values()->begin(),
               dedup_value.mutable_array_value()->mutable_values()->end());
  value_ = std::move(dedup_value);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
