// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_METADATA_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_METADATA_H_

#include "google/cloud/status_or.h"
#include "google/cloud/storage/internal/nljson.h"
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents the metadata for a Google Cloud Storage Notification resource.
 *
 * Notifications send information about changes to objects in your buckets to
 * Cloud Pub/Sub.
 *
 * @see https://cloud.google.com/storage/docs/pubsub-notifications for general
 *     information on Google Cloud Storage Notifications.
 * @see https://cloud.google.com/pubsub/ for general information on Google
 *     Cloud Pub/Sub service.
 */
class NotificationMetadata {
 public:
  NotificationMetadata() = default;

  static StatusOr<NotificationMetadata> ParseFromJson(
      internal::nl::json const& json);
  static StatusOr<NotificationMetadata> ParseFromString(
      std::string const& payload);

  /**
   * Returns the payload for a call to `Notifications: insert`.
   */
  std::string JsonPayloadForInsert() const;

  //@{
  /// @name Accessors and modifiers to the custom attributes.
  bool has_custom_attribute(std::string const& key) const {
    return custom_attributes_.end() != custom_attributes_.find(key);
  }

  std::string const& custom_attribute(std::string const& key) const {
    return custom_attributes_.at(key);
  }

  /// Delete a custom attribute. This is a no-op if the key does not exist.
  NotificationMetadata& delete_custom_attribute(std::string const& key) {
    auto i = custom_attributes_.find(key);
    if (i == custom_attributes_.end()) {
      return *this;
    }
    custom_attributes_.erase(i);
    return *this;
  }

  /// Insert or update the custom attribute.
  NotificationMetadata& upsert_custom_attributes(std::string key,
                                                 std::string value) {
    auto i = custom_attributes_.lower_bound(key);
    if (i == custom_attributes_.end() or i->first != key) {
      custom_attributes_.emplace_hint(i, std::move(key), std::move(value));
    } else {
      i->second = std::move(value);
    }
    return *this;
  }

  /// Full accessors for the custom attributes.
  std::map<std::string, std::string> const& custom_attributes() const {
    return custom_attributes_;
  }
  std::map<std::string, std::string>& mutable_custom_attributes() {
    return custom_attributes_;
  }
  //@}

  std::string const& etag() const { return etag_; }

  //@{
  /**
   * @name Accessors and modifiers to the event types list.
   *
   * Define the list of event types that this notification will include.
   *
   * @see https://cloud.google.com/storage/docs/pubsub-notifications#events for
   *     a description of valid even types.
   */
  std::size_t event_type_size() const { return event_types_.size(); }
  std::string const& event_type(std::size_t index) const {
    return event_types_.at(index);
  }

  NotificationMetadata& append_event_type(std::string e) {
    event_types_.emplace_back(std::move(e));
    return *this;
  }

  std::vector<std::string> const& event_types() const { return event_types_; }
  std::vector<std::string>& mutable_event_types() { return event_types_; }
  //@}

  std::string const& id() const { return id_; }
  std::string const& kind() const { return kind_; }

  std::string const& object_name_prefix() const { return object_name_prefix_; }
  NotificationMetadata& set_object_name_prefix(std::string v) {
    object_name_prefix_ = std::move(v);
    return *this;
  }

  std::string const& payload_format() const { return payload_format_; }
  NotificationMetadata& set_payload_format(std::string v) {
    payload_format_ = std::move(v);
    return *this;
  }

  std::string const& self_link() const { return self_link_; }

  std::string const& topic() const { return topic_; }
  NotificationMetadata& set_topic(std::string v) {
    topic_ = std::move(v);
    return *this;
  }

  bool operator==(NotificationMetadata const& rhs) const {
    return std::tie(id_, custom_attributes_, etag_, event_types_, kind_,
                    object_name_prefix_, payload_format_, self_link_, topic_) ==
           std::tie(rhs.id_, rhs.custom_attributes_, rhs.etag_,
                    rhs.event_types_, rhs.kind_, rhs.object_name_prefix_,
                    rhs.payload_format_, rhs.self_link_, rhs.topic_);
  }

  bool operator<(NotificationMetadata const& rhs) const {
    return std::tie(id_, custom_attributes_, etag_, event_types_, kind_,
                    object_name_prefix_, payload_format_, self_link_, topic_) <
           std::tie(rhs.id_, rhs.custom_attributes_, rhs.etag_,
                    rhs.event_types_, rhs.kind_, rhs.object_name_prefix_,
                    rhs.payload_format_, rhs.self_link_, rhs.topic_);
  }

  bool operator!=(NotificationMetadata const& rhs) const {
    return std::rel_ops::operator!=(*this, rhs);
  }

  bool operator>(NotificationMetadata const& rhs) const {
    return std::rel_ops::operator>(*this, rhs);
  }

  bool operator<=(NotificationMetadata const& rhs) const {
    return std::rel_ops::operator<=(*this, rhs);
  }

  bool operator>=(NotificationMetadata const& rhs) const {
    return std::rel_ops::operator>=(*this, rhs);
  }

 private:
  friend std::ostream& operator<<(std::ostream& os,
                                  NotificationMetadata const& rhs);

  // Keep the fields in alphabetical order.
  std::map<std::string, std::string> custom_attributes_;
  std::string etag_;
  std::vector<std::string> event_types_;
  std::string id_;
  std::string kind_;
  std::string object_name_prefix_;
  std::string payload_format_;
  std::string self_link_;
  std::string topic_;
};

std::ostream& operator<<(std::ostream& os, NotificationMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_METADATA_H_
