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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_CREATE_TOPIC_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_CREATE_TOPIC_BUILDER_H

#include "google/cloud/pubsub/topic.h"
#include "google/cloud/pubsub/version.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Create a Cloud Pub/Sub topic configuration.
 */
class CreateTopicBuilder {
 public:
  explicit CreateTopicBuilder(Topic const& topic) {
    proto_.set_name(topic.FullName());
  }

  CreateTopicBuilder& add_label(std::string const& key,
                                std::string const& value) {
    using value_type = protobuf::Map<std::string, std::string>::value_type;
    proto_.mutable_labels()->insert(value_type(key, value));
    return *this;
  }

  CreateTopicBuilder& clear_labels() {
    proto_.clear_labels();
    return *this;
  }

  CreateTopicBuilder& add_allowed_persistence_region(std::string region) {
    proto_.mutable_message_storage_policy()->add_allowed_persistence_regions(
        std::move(region));
    return *this;
  }
  CreateTopicBuilder& clear_allowed_persistence_regions() {
    proto_.mutable_message_storage_policy()
        ->clear_allowed_persistence_regions();
    return *this;
  }

  CreateTopicBuilder& set_kms_key_name(std::string key_name) {
    proto_.set_kms_key_name(std::move(key_name));
    return *this;
  }

  google::pubsub::v1::Topic as_proto() const& { return proto_; }
  google::pubsub::v1::Topic&& as_proto() && { return std::move(proto_); }

 private:
  google::pubsub::v1::Topic proto_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_CREATE_TOPIC_BUILDER_H
