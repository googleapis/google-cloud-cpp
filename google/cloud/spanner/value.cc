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

#include "google/cloud/spanner/value.h"
#include "google/cloud/log.h"
#include <google/protobuf/util/field_comparator.h>
#include <google/protobuf/util/message_differencer.h>
#include <cmath>
#include <ios>
#include <string>

// Implementation note: See
// https://github.com/googleapis/googleapis/blob/master/google/spanner/v1/type.proto
// for details about how Spanner types should be encoded.

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

// Compares two sets of Type and Value protos for equality. This method calls
// itself recursively to compare subtypes and subvalues.
bool Equal(google::spanner::v1::Type const& pt1,
           google::protobuf::Value const& pv1,
           google::spanner::v1::Type const& pt2,
           google::protobuf::Value const& pv2) {
  if (pt1.code() != pt2.code()) return false;
  if (pv1.kind_case() != pv2.kind_case()) return false;
  switch (pt1.code()) {
    case google::spanner::v1::TypeCode::BOOL:
      return pv1.bool_value() == pv2.bool_value();
    case google::spanner::v1::TypeCode::INT64:
      return pv1.string_value() == pv2.string_value();
    case google::spanner::v1::TypeCode::FLOAT64:
      // NaN should always compare not equal, even to itself.
      if (pv1.string_value() == "NaN" || pv2.string_value() == "NaN")
        return false;
      return pv1.string_value() == pv2.string_value() &&
             pv1.number_value() == pv2.number_value();
    case google::spanner::v1::TypeCode::STRING:
      return pv1.string_value() == pv2.string_value();
    case google::spanner::v1::TypeCode::ARRAY: {
      auto const& etype1 = pt1.array_element_type();
      auto const& etype2 = pt2.array_element_type();
      if (etype1.code() != etype2.code()) return false;
      auto const& v1 = pv1.list_value().values();
      auto const& v2 = pv2.list_value().values();
      if (v1.size() != v2.size()) return false;
      for (int i = 0; i < v1.size(); ++i) {
        if (!Equal(etype1, v1.Get(i), etype1, v2.Get(i))) {
          return false;
        }
      }
    }
    default:
      return true;
  }
}

}  // namespace

Value::Value(bool v) {
  type_.set_code(google::spanner::v1::TypeCode::BOOL);
  value_.set_bool_value(v);
}

Value::Value(std::int64_t v) {
  type_.set_code(google::spanner::v1::TypeCode::INT64);
  value_.set_string_value(std::to_string(v));
}

Value::Value(double v) {
  type_.set_code(google::spanner::v1::TypeCode::FLOAT64);
  if (std::isnan(v)) {
    value_.set_string_value("NaN");
  } else if (std::isinf(v)) {
    value_.set_string_value(v < 0 ? "-Infinity" : "Infinity");
  } else {
    value_.set_number_value(v);
  }
}

Value::Value(std::string v) {
  type_.set_code(google::spanner::v1::TypeCode::STRING);
  value_.set_string_value(std::move(v));
}

bool operator==(Value a, Value b) {
  return Equal(a.type_, a.value_, b.type_, b.value_);
}

void PrintTo(Value const& v, std::ostream* os) {
  *os << v.type_.ShortDebugString() << "; " << v.value_.ShortDebugString();
}

bool Value::ProtoEqual(google::protobuf::Message const& m1,
                       google::protobuf::Message const& m2) {
  return google::protobuf::util::MessageDifferencer::Equals(m1, m2);
}

//
// Value::GetType
//

google::spanner::v1::Type Value::GetType(bool) {
  google::spanner::v1::Type t;
  t.set_code(google::spanner::v1::TypeCode::BOOL);
  return t;
}

google::spanner::v1::Type Value::GetType(std::int64_t) {
  google::spanner::v1::Type t;
  t.set_code(google::spanner::v1::TypeCode::INT64);
  return t;
}

google::spanner::v1::Type Value::GetType(double) {
  google::spanner::v1::Type t;
  t.set_code(google::spanner::v1::TypeCode::FLOAT64);
  return t;
}

google::spanner::v1::Type Value::GetType(std::string const&) {
  google::spanner::v1::Type t;
  t.set_code(google::spanner::v1::TypeCode::STRING);
  return t;
}

//
// Value::GetValue
//

bool Value::GetValue(bool, google::protobuf::Value const& pv) {
  return pv.bool_value();
}

std::int64_t Value::GetValue(std::int64_t, google::protobuf::Value const& pv) {
  auto const& s = pv.string_value();
  std::size_t processed = 0;
  long long x = std::stoll(s, &processed, 10);
  if (processed != s.size()) {
    GCP_LOG(FATAL) << "Failed to parse number from string: \"" << s << "\"";
  }
  return {x};
}

double Value::GetValue(double, google::protobuf::Value const& pv) {
  if (pv.kind_case() == google::protobuf::Value::kStringValue) {
    std::string const& s = pv.string_value();
    auto const inf = std::numeric_limits<double>::infinity();
    if (s == "-Infinity") return -inf;
    if (s == "Infinity") return inf;
    return std::nan(s.c_str());
  }
  return pv.number_value();
}

std::string Value::GetValue(std::string const&,
                            google::protobuf::Value const& pv) {
  return pv.string_value();
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
