// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/message.h"
#include "google/cloud/internal/time_utils.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include <iostream>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

::google::pubsub::v1::PubsubMessage const& ToProto(pubsub::Message const& m) {
  return m.proto_;
}

::google::pubsub::v1::PubsubMessage&& ToProto(pubsub::Message&& m) {
  return std::move(m.proto_);
}

pubsub::Message FromProto(::google::pubsub::v1::PubsubMessage m) {
  return pubsub::Message(std::move(m));
}

std::size_t MessageSize(pubsub::Message const& m) { return m.MessageSize(); }

std::size_t MessageProtoSize(::google::pubsub::v1::PubsubMessage const& m) {
  // see https://cloud.google.com/pubsub/pricing
  auto constexpr kTimestampOverhead = 20;
  std::size_t s = kTimestampOverhead + m.data().size();
  s += m.message_id().size();
  s += m.ordering_key().size();
  for (auto const& kv : m.attributes()) {
    s += kv.first.size() + kv.second.size();
  }
  return s;
}

void SetAttribute(std::string const& key, std::string value,
                  pubsub::Message& m) {
  (*m.proto_.mutable_attributes())[key] = std::move(value);
}

absl::string_view GetAttribute(std::string const& key, pubsub::Message& m) {
  auto value = m.proto_.attributes().find(key);
  if (value != m.proto_.attributes().end()) {
    return absl::string_view(value->second.data(), value->second.size());
  }
  return absl::string_view{};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal

namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::chrono::system_clock::time_point Message::publish_time() const {
  return google::cloud::internal::ToChronoTimePoint(proto_.publish_time());
}

std::size_t Message::MessageSize() const {
  return pubsub_internal::MessageProtoSize(proto_);
}

bool operator==(Message const& a, Message const& b) {
  google::protobuf::util::MessageDifferencer diff;
  return diff.Compare(a.proto_, b.proto_);
}

std::ostream& operator<<(std::ostream& os, Message const& rhs) {
  auto constexpr kMaximumPayloadBytes = 64;
  google::protobuf::TextFormat::Printer p;
  p.SetSingleLineMode(true);
  p.SetTruncateStringFieldLongerThan(kMaximumPayloadBytes);
  std::string text;
  p.PrintToString(rhs.proto_, &text);
  return os << text;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
