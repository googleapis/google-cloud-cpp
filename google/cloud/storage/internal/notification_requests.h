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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NOTIFICATION_REQUESTS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NOTIFICATION_REQUESTS_H_

#include "google/cloud/storage/internal/generic_request.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/notification_metadata.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct NotificationMetadataParser {
  static StatusOr<NotificationMetadata> FromJson(
      internal::nl::json const& json);
  static StatusOr<NotificationMetadata> FromString(std::string const& payload);
};

/// Represents a request to call the `BucketAccessControls: list` API.
class ListNotificationsRequest
    : public GenericRequest<ListNotificationsRequest, UserProject> {
 public:
  ListNotificationsRequest() : GenericRequest() {}
  explicit ListNotificationsRequest(std::string bucket)
      : bucket_name_(std::move(bucket)) {}

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

std::ostream& operator<<(std::ostream& os, ListNotificationsRequest const& r);

/// Represents a response to the `Notification: list` API.
struct ListNotificationsResponse {
  static StatusOr<ListNotificationsResponse> FromHttpResponse(
      HttpResponse&& response);

  std::vector<NotificationMetadata> items;
};

std::ostream& operator<<(std::ostream& os, ListNotificationsResponse const& r);

/**
 * Represents a request to call the `Notifications: insert` API.
 */
class CreateNotificationRequest
    : public GenericRequest<CreateNotificationRequest, UserProject> {
 public:
  CreateNotificationRequest() = default;
  CreateNotificationRequest(std::string bucket,
                            NotificationMetadata const& notification)
      : bucket_name_(std::move(bucket)),
        json_payload_(notification.JsonPayloadForInsert()) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& json_payload() const { return json_payload_; }

 private:
  std::string bucket_name_;
  std::string json_payload_;
};

std::ostream& operator<<(std::ostream& os, CreateNotificationRequest const& r);

/**
 * Represents common attributes to multiple `Notifications` request
 * types.
 *
 * The classes to represent requests for the `Notifications: create`,
 * `get`, `delete`, `patch`, and `update` APIs have a lot of commonality. This
 * template class refactors that code.
 */
template <typename Derived>
class GenericNotificationRequest : public GenericRequest<Derived, UserProject> {
 public:
  GenericNotificationRequest() = default;
  GenericNotificationRequest(std::string bucket, std::string notification_id)
      : bucket_name_(std::move(bucket)),
        notification_id_(std::move(notification_id)) {}

  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& notification_id() const { return notification_id_; }

 private:
  std::string bucket_name_;
  std::string notification_id_;
};

/**
 * Represents a request to call the `Notifications: get` API.
 */
class GetNotificationRequest
    : public GenericNotificationRequest<GetNotificationRequest> {
  using GenericNotificationRequest::GenericNotificationRequest;
};

std::ostream& operator<<(std::ostream& os, GetNotificationRequest const& r);

/**
 * Represents a request to call the `Notifications: delete` API.
 */
class DeleteNotificationRequest
    : public GenericNotificationRequest<DeleteNotificationRequest> {
  using GenericNotificationRequest::GenericNotificationRequest;
};

std::ostream& operator<<(std::ostream& os, DeleteNotificationRequest const& r);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NOTIFICATION_REQUESTS_H_
