// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PROTO_ENUM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PROTO_ENUM_H

#include "google/cloud/version.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the Spanner ENUM type: a protobuf enumeration.
 *
 * A `ProtoEnum<E>` can be implicitly constructed from, and explicitly
 * converted to an `E`.  Values can be copied, assigned, compared for
 * equality, and streamed.
 *
 * @par Example
 * Given a proto definition `enum Color { RED = 0; BLUE = 1; GREEN = 2; }`:
 * @code
 * auto e = spanner::ProtoEnum<Color>(BLUE);
 * assert(Color(e) == BLUE);
 * @endcode
 */
template <typename E>
class ProtoEnum {
 public:
  using enum_type = E;

  /// The default value is the first listed in the enum's definition.
  ProtoEnum()
      : ProtoEnum(static_cast<enum_type>(Descriptor()->value(0)->number())) {}

  /// Implicit construction from the enum type.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ProtoEnum(enum_type v) : v_(v) {}

  /// Explicit conversion to the enum type.
  explicit operator enum_type() const { return v_; }

  /// The fully-qualified name of the enum type, scope delimited by periods.
  static std::string const& TypeName() { return Descriptor()->full_name(); }

  /// @name Relational operators
  ///@{
  friend bool operator==(ProtoEnum const& a, ProtoEnum const& b) {
    return a.v_ == b.v_;
  }
  friend bool operator!=(ProtoEnum const& a, ProtoEnum const& b) {
    return !(a == b);
  }
  ///@}

  /// Outputs string representation of the `ProtoEnum` to the stream.
  friend std::ostream& operator<<(std::ostream& os, ProtoEnum const& v) {
    auto const* ed = Descriptor();
    if (static_cast<int>(v.v_) == v.v_) {
      if (auto const* vd = ed->FindValueByNumber(static_cast<int>(v.v_))) {
        return os << vd->full_name();
      }
    }
    return os << ed->full_name() << ".{" << v.v_ << "}";
  }

 private:
  static google::protobuf::EnumDescriptor const* Descriptor() {
    return google::protobuf::GetEnumDescriptor<enum_type>();
  }

  enum_type v_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PROTO_ENUM_H
