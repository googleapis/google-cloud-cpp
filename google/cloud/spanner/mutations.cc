// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/mutations.h"
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/message_differencer.h>
#include <iostream>
#include <utility>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

SendMutationBuilder::SendMutationBuilder(std::string queue, spanner::Key key,
                                         spanner::Value payload) {
  google::spanner::v1::Mutation::Send& send = *m_.proto().mutable_send();
  send.set_queue(std::move(queue));
  google::protobuf::ListValue& key_proto = *send.mutable_key();
  key_proto.mutable_values()->Reserve(static_cast<int>(key.size()));
  for (spanner::Value& k : key) {
    *key_proto.add_values() = spanner_internal::ToProto(std::move(k)).second;
  }
  *send.mutable_payload() =
      spanner_internal::ToProto(std::move(payload)).second;
}

SendMutationBuilder& SendMutationBuilder::SetDeliverTime(
    spanner::Timestamp deliver_time) & {
  *m_.proto().mutable_send()->mutable_deliver_time() =
      deliver_time.get<google::protobuf::Timestamp>().value();
  return *this;
}

AckMutationBuilder::AckMutationBuilder(std::string queue, spanner::Key key) {
  google::spanner::v1::Mutation::Ack& ack = *m_.proto().mutable_ack();
  ack.set_queue(std::move(queue));
  google::protobuf::ListValue& key_proto = *ack.mutable_key();
  key_proto.mutable_values()->Reserve(static_cast<int>(key.size()));
  for (spanner::Value& k : key) {
    *key_proto.add_values() = spanner_internal::ToProto(std::move(k)).second;
  }
}

AckMutationBuilder& AckMutationBuilder::SetIgnoreNotFound(
    bool ignore_not_found) & {
  m_.proto().mutable_ack()->set_ignore_not_found(ignore_not_found);
  return *this;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool operator==(Mutation const& lhs, Mutation const& rhs) {
  google::protobuf::util::MessageDifferencer diff;
  return diff.Compare(lhs.m_, rhs.m_);
}

void PrintTo(Mutation const& m, std::ostream* os) {
  *os << "Mutation={" << m.m_.DebugString() << "}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
