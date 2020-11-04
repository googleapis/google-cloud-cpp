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

/// Estimate the size of a message.
std::size_t MessageSize(pubsub::Message const&);
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal

namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
class MessageBuilder;

/**
 * Defines the type for message data.
 *
 * Inside Google some protobuf fields of type `bytes` are mapped to a different
 * type than `std::string`. This is the case for message data. We use this
 * type to automatically detect what is the representation for this field and
 * use the correct mapping.
 *
 * External users of the Cloud Pubsub C++ client library should treat this as
 * a complicated `typedef` for `std::string`. We have no plans to change the
 * type in the external version of the C++ client library for the foreseeable
 * future. In the eventuality that we do decide to change the type, this would
 * be a reason update the library major version number, and we would give users
 * time to migrate.
 *
 * In other words, external users of the Cloud Pubsub C++ client should simply
 * write `std::string` where this type appears. For Google projects that must
 * compile both inside and outside Google, this alias may be convenient.
 */
using PubsubMessageDataType = std::decay<decltype(
    std::declval<google::pubsub::v1::PubsubMessage>().data())>::type;

/**
 * The C++ representation for a Cloud Pub/Sub messages.
 *
 * Cloud Pub/Sub applications communicate to each other using messages. Note
 * that messages must provide at least some data or some attributes. Use
 * `MessageBuilder` to create instances of this class.
 */
class Message {
 public:
  //@{
  /// @name accessors
  PubsubMessageDataType const& data() const& { return proto_.data(); }
  PubsubMessageDataType&& data() && {
    return std::move(*proto_.mutable_data());
  }
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

  //@{
  /// @name Copy and move
  Message(Message const&) = default;
  Message& operator=(Message const&) = default;
  Message(Message&&) = default;
  Message& operator=(Message&&) = default;
  //@}

  //@{
  /// @name Equality operators
  /// Compares two messages.
  friend bool operator==(Message const& a, Message const& b);
  /// Compares two messages.
  friend bool operator!=(Message const& a, Message const& b) {
    return !(a == b);
  }
  //@}

  /// Output in protobuf format, this is intended for debugging
  friend std::ostream& operator<<(std::ostream& os, Message const& rhs);

 private:
  friend Message pubsub_internal::FromProto(
      ::google::pubsub::v1::PubsubMessage m);
  friend ::google::pubsub::v1::PubsubMessage const& pubsub_internal::ToProto(
      Message const& m);
  friend ::google::pubsub::v1::PubsubMessage&& pubsub_internal::ToProto(
      Message&& m);
  friend std::size_t pubsub_internal::MessageSize(Message const&);

  /// Construct `Message` objects.
  friend class MessageBuilder;

  Message() = default;

  explicit Message(::google::pubsub::v1::PubsubMessage m)
      : proto_(std::move(m)) {}

  std::size_t MessageSize() const;

  google::pubsub::v1::PubsubMessage proto_;
};

/** @relates Message
 * Constructs `Message` objects.
 */
class MessageBuilder {
 public:
  MessageBuilder() = default;

  /// Creates a new message.
  Message Build() && { return Message(std::move(proto_)); }

  /// Sets the message payload to @p data
  MessageBuilder& SetData(std::string data) & {
    proto_.set_data(std::move(data));
    return *this;
  }

  /// Sets the message payload to @p data
  MessageBuilder&& SetData(std::string data) && {
    SetData(std::move(data));
    return std::move(*this);
  }

  /// Sets the ordering key to @p key
  MessageBuilder& SetOrderingKey(std::string key) & {
    proto_.set_ordering_key(std::move(key));
    return *this;
  }

  /// Sets the ordering key to @p key
  MessageBuilder&& SetOrderingKey(std::string key) && {
    return std::move(SetOrderingKey(std::move(key)));
  }

  /// Inserts an attribute to the message, leaving the message unchanged if @p
  /// key is already present.
  MessageBuilder& InsertAttribute(std::string const& key,
                                  std::string const& value) & {
    using value_type =
        google::protobuf::Map<std::string, std::string>::value_type;
    proto_.mutable_attributes()->insert(value_type{key, value});
    return *this;
  }

  /// Inserts an attribute to the message, leaving the message unchanged if @p
  /// key is already present.
  MessageBuilder&& InsertAttribute(std::string const& key,
                                   std::string const& value) && {
    return std::move(InsertAttribute(key, value));
  }

  /// Inserts or sets an attribute on the message.
  MessageBuilder& SetAttribute(std::string const& key, std::string value) & {
    (*proto_.mutable_attributes())[key] = std::move(value);
    return *this;
  }

  /// Inserts or sets an attribute on the message.
  MessageBuilder&& SetAttribute(std::string const& key, std::string value) && {
    return std::move(SetAttribute(key, std::move(value)));
  }

  /// Sets the attributes in the message to the attributes from the range [@p
  /// begin, @p end)
  template <typename Iterator>
  MessageBuilder& SetAttributes(Iterator begin, Iterator end) & {
    google::protobuf::Map<std::string, std::string> tmp;
    using value_type =
        google::protobuf::Map<std::string, std::string>::value_type;
    for (auto kv = begin; kv != end; ++kv) {
      using std::get;
      tmp.insert(value_type(get<0>(*kv), get<1>(*kv)));
    }
    proto_.mutable_attributes()->swap(tmp);
    return *this;
  }

  /// Sets the attributes in the message to the attributes from the range [@p
  /// begin, @p end)
  template <typename Iterator>
  MessageBuilder&& SetAttributes(Iterator begin, Iterator end) && {
    SetAttributes(std::move(begin), std::move(end));
    return std::move(*this);
  }

  /// Sets the attributes in the message to @p v
  MessageBuilder& SetAttributes(
      // NOLINTNEXTLINE(performance-unnecessary-value-param)
      std::vector<std::pair<std::string, std::string>> v) & {
    using value_type =
        google::protobuf::Map<std::string, std::string>::value_type;
    google::protobuf::Map<std::string, std::string> tmp;
    for (auto& kv : v) {
      tmp.insert(value_type(std::move(kv.first), std::move(kv.second)));
    }
    proto_.mutable_attributes()->swap(tmp);
    return *this;
  }

  /// Sets the attributes in the message to @p v
  MessageBuilder&& SetAttributes(
      // NOLINTNEXTLINE(performance-unnecessary-value-param)
      std::vector<std::pair<std::string, std::string>> v) && {
    SetAttributes(std::move(v));
    return std::move(*this);
  }

  /// Sets the attributes in the message to @p v
  template <typename Pair>
  MessageBuilder& SetAttributes(std::vector<Pair> v) & {
    return SetAttributes(v.begin(), v.end());
  }

  /// Sets the attributes in the message to @p v
  template <typename Pair>
  MessageBuilder&& SetAttributes(std::vector<Pair> v) && {
    SetAttributes(std::move(v));
    return std::move(*this);
  }

 private:
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

inline std::size_t MessageSize(pubsub::Message const& m) {
  return m.MessageSize();
}

std::size_t MessageProtoSize(::google::pubsub::v1::PubsubMessage const& m);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_MESSAGE_H
