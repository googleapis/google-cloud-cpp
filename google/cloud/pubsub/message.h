// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MESSAGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MESSAGE_H

#include "google/cloud/pubsub/version.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <iosfwd>
#include <map>
#include <tuple>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
class Message;
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
::google::pubsub::v1::PubsubMessage const& ToProto(pubsub::Message const&);
::google::pubsub::v1::PubsubMessage&& ToProto(pubsub::Message&&);
pubsub::Message FromProto(::google::pubsub::v1::PubsubMessage);
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal

namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * The C++ representation for a Cloud Pub/Sub messages.
 *
 * Cloud Pub/Sub applications communicate to each other using messages. Note
 * that messages must provide at least some data or some attributes. The `From*`
 * factory functions are designed to encourage applications to always create
 * valid messages.
 *
 * A #Message is immutable other than through move copy or assignment. Once a
 * message is created its properties cannot be changed, though it is possible to
 * create new messages by using the `Set*()` factory functions.
 */
class Message {
 public:
  /// Change the payload in @p m to @p data
  static Message SetData(Message m, std::string data) {
    m.proto_.set_data(std::move(data));
    return m;
  }

  /// Change the attributes in @p m to those in the range [@p begin, @p end)
  template <typename Iterator>
  static Message SetAttributes(Message m, Iterator begin, Iterator end) {
    m.proto_.clear_attributes();
    for (auto kv = begin; kv != end; ++kv) {
      using std::get;
      (*m.proto_.mutable_attributes())[get<0>(*kv)] = get<1>(*kv);
    }
    return m;
  }

  /// Change the attributes in @p m to the values in @p v
  static Message SetAttributes(
      Message m, std::vector<std::pair<std::string, std::string>> v) {
    using value_type =
        google::protobuf::Map<std::string, std::string>::value_type;
    m.proto_.clear_attributes();
    for (auto& kv : std::move(v)) {
      m.proto_.mutable_attributes()->insert(
          value_type(std::move(kv.first), std::move(kv.second)));
    }
    return m;
  }

  /// Change the attributes in @p m to the values in @p v
  template <typename Pair>
  static Message SetAttributes(Message m, std::vector<Pair> v) {
    return SetAttributes(std::move(m), v.begin(), v.end());
  }

  /// Create a new message with @p data as its payload
  static Message FromData(std::string data) {
    return SetData(Message{}, std::move(data));
  }

  /// Create a new message with the attributes in the range [@p begin, @p end)
  template <typename Iterator>
  static Message FromAttributes(Iterator begin, Iterator end) {
    return SetAttributes(Message{}, begin, end);
  }

  /// Create a new message with the attributes in @p v
  static Message FromAttributes(
      std::vector<std::pair<std::string, std::string>> v) {
    return SetAttributes(Message{}, std::move(v));
  }

  /// Create a new message with the attributes in @p v
  template <typename Pair>
  static Message FromAttributes(std::vector<Pair> v) {
    return SetAttributes(Message{}, std::move(v));
  }

  /// @name accessors
  //@{
  std::string const& data() const& { return proto_.data(); }
  std::string&& data() && { return std::move(*proto_.mutable_data()); }
  std::string const& message_id() const { return proto_.message_id(); }
  std::string const& ordering_key() const { return proto_.ordering_key(); }
  std::chrono::system_clock::time_point publish_time() const;
  std::map<std::string, std::string> attributes() const {
    std::map<std::string, std::string> r;
    for (auto const& kv : proto_.attributes()) {
      r.emplace(kv.first, kv.second);
    }
    return r;
  }
  //@}

  /// @name Copy and move
  //@{
  Message(Message const&) = default;
  Message& operator=(Message const&) = default;
  Message(Message&&) = default;
  Message& operator=(Message&&) = default;
  //@}

  /// @name Equality operators
  //@{
  friend bool operator==(Message const& a, Message const& b);
  friend bool operator!=(Message const& a, Message const& b) {
    return !(a == b);
  }
  //@}

  /// Output in protobuf format, this is intended for debugging
  friend std::ostream& operator<<(std::ostream& os, Message const& rhs);

 private:
  Message() = default;

  Message(::google::pubsub::v1::PubsubMessage m) : proto_(std::move(m)) {}

  friend Message pubsub_internal::FromProto(
      ::google::pubsub::v1::PubsubMessage m);
  friend ::google::pubsub::v1::PubsubMessage const& pubsub_internal::ToProto(
      Message const& m);
  friend ::google::pubsub::v1::PubsubMessage&& pubsub_internal::ToProto(
      Message&& m);
  google::pubsub::v1::PubsubMessage proto_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
inline ::google::pubsub::v1::PubsubMessage const& ToProto(
    pubsub::Message const& m) {
  return m.proto_;
}

inline ::google::pubsub::v1::PubsubMessage&& ToProto(pubsub::Message&& m) {
  return std::move(m.proto_);
}

inline pubsub::Message FromProto(::google::pubsub::v1::PubsubMessage m) {
  return pubsub::Message(std::move(m));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal

}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MESSAGE_H
