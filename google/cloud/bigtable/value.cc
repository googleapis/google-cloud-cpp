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
#include <google/bigtable/v2/types.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
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

//
// Value::MakeTypeProto
//

google::bigtable::v2::Type Value::MakeTypeProto(bool) {
  google::bigtable::v2::Type t;
  t.set_allocated_bool_type(std::move(new google::bigtable::v2::Type_Bool()));
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

bool Value::is_null() const { return IsNullValue(value_); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
