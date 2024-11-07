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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PROTO_MESSAGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PROTO_MESSAGE_H

#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the Spanner PROTO type: a protobuf message.
 *
 * A `ProtoMessage<M>` can be implicitly constructed from and explicitly
 * converted to an `M`.  Values can be copied, assigned, and streamed.
 *
 * A `ProtoMessage<M>` can also be explicitly constructed from and converted
 * to the `M` wire format, although this is intended for internal use only.
 *
 * @par Example
 * Given a proto definition `message Mesg { string field = 1; }`:
 * @code
 * Mesg m;
 * m.set_field("value");
 * auto pm = spanner::ProtoMessage<Mesg>(m);
 * assert(Mesg(pm).field() == "value");
 * @endcode
 */
template <typename M>
class ProtoMessage {
 public:
  using message_type = M;

  /// The default value.
  ProtoMessage() : ProtoMessage(message_type::default_instance()) {}

  /// Implicit construction from the message type.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ProtoMessage(M const& m) { m.SerializeToString(&serialized_message_); }

  /// Explicit construction from wire format.
  explicit ProtoMessage(std::string serialized_message)
      : serialized_message_(std::move(serialized_message)) {}

  /// Explicit conversion to the message type.
  explicit operator message_type() const {
    message_type m;
    m.ParseFromString(serialized_message_);
    return m;
  }

  /// Explicit conversion to wire format.
  explicit operator std::string() const { return serialized_message_; }

  /// The fully-qualified name of the message type, scope delimited by periods.
  static absl::string_view TypeName() {
    return message_type::GetDescriptor()->full_name();
  }

  /// @name Relational operators
  ///@{
  friend bool operator==(ProtoMessage const& a, ProtoMessage const& b) {
    if (a.serialized_message_ == b.serialized_message_) return true;
    return google::protobuf::util::MessageDifferencer::Equivalent(
        message_type(a), message_type(b));
  }
  friend bool operator!=(ProtoMessage const& a, ProtoMessage const& b) {
    return !(a == b);
  }
  ///@}

  /// Outputs string representation of the `ProtoMessage` to the stream.
  friend std::ostream& operator<<(std::ostream& os, ProtoMessage const& m) {
    return os << internal::DebugString(message_type(m), TracingOptions{});
  }

 private:
  std::string serialized_message_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_PROTO_MESSAGE_H
