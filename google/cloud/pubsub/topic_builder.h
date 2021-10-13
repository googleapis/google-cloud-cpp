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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_BUILDER_H

#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/topic.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/time_utils.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Builds requests to create or update a Cloud Pub/Sub topic.
 */
class TopicBuilder {
 public:
  explicit TopicBuilder(Topic const& topic) {
    proto_.set_name(topic.FullName());
  }

  google::pubsub::v1::Topic BuildCreateRequest() &&;

  google::pubsub::v1::UpdateTopicRequest BuildUpdateRequest() &&;

  TopicBuilder& add_label(std::string const& key, std::string const& value) & {
    using value_type = protobuf::Map<std::string, std::string>::value_type;
    proto_.mutable_labels()->insert(value_type(key, value));
    paths_.insert("labels");
    return *this;
  }
  TopicBuilder&& add_label(std::string const& key,
                           std::string const& value) && {
    return std::move(add_label(key, value));
  }

  TopicBuilder& clear_labels() & {
    proto_.clear_labels();
    paths_.insert("labels");
    return *this;
  }
  TopicBuilder&& clear_labels() && { return std::move(clear_labels()); }

  TopicBuilder& add_allowed_persistence_region(std::string region) & {
    proto_.mutable_message_storage_policy()->add_allowed_persistence_regions(
        std::move(region));
    paths_.insert("message_storage_policy");
    return *this;
  }
  TopicBuilder&& add_allowed_persistence_region(std::string region) && {
    return std::move(add_allowed_persistence_region(std::move(region)));
  }

  TopicBuilder& clear_allowed_persistence_regions() & {
    proto_.mutable_message_storage_policy()
        ->clear_allowed_persistence_regions();
    paths_.insert("message_storage_policy");
    return *this;
  }
  TopicBuilder&& clear_allowed_persistence_regions() && {
    return std::move(clear_allowed_persistence_regions());
  }

  TopicBuilder& set_kms_key_name(std::string key_name) & {
    proto_.set_kms_key_name(std::move(key_name));
    paths_.insert("kms_key_name");
    return *this;
  }
  TopicBuilder&& set_kms_key_name(std::string key_name) && {
    return std::move(set_kms_key_name(std::move(key_name)));
  }

  TopicBuilder& set_schema(pubsub::Schema const& schema) & {
    proto_.mutable_schema_settings()->set_schema(schema.FullName());
    paths_.insert("schema_settings.schema");
    return *this;
  }
  TopicBuilder&& set_schema(pubsub::Schema const& schema) && {
    return std::move(set_schema(schema));
  }
  TopicBuilder& set_encoding(google::pubsub::v1::Encoding encoding) & {
    proto_.mutable_schema_settings()->set_encoding(encoding);
    paths_.insert("schema_settings.encoding");
    return *this;
  }
  TopicBuilder&& set_encoding(google::pubsub::v1::Encoding encoding) && {
    return std::move(set_encoding(encoding));
  }

  template <typename Rep, typename Period>
  TopicBuilder& set_message_retention_duration(
      std::chrono::duration<Rep, Period> d) & {
    *proto_.mutable_message_retention_duration() =
        google::cloud::internal::ToDurationProto(d);
    paths_.insert("message_retention_duration");
    return *this;
  }
  template <typename Rep, typename Period>
  TopicBuilder&& set_message_retention_duration(
      std::chrono::duration<Rep, Period> d) && {
    return std::move(set_message_retention_duration(d));
  }
  TopicBuilder& set_message_retention_duration(
      google::protobuf::Duration const& d) & {
    *proto_.mutable_message_retention_duration() = d;
    paths_.insert("message_retention_duration");
    return *this;
  }
  TopicBuilder&& set_message_retention_duration(
      google::protobuf::Duration const& d) && {
    return std::move(set_message_retention_duration(d));
  }

 private:
  google::pubsub::v1::Topic proto_;
  std::set<std::string> paths_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_BUILDER_H
