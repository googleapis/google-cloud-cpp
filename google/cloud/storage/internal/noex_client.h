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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NOEX_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NOEX_CLIENT_H_

#include "google/cloud/internal/disjunction.h"
#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/list_buckets_reader.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/notification_event_type.h"
#include "google/cloud/storage/notification_payload_format.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/object_rewriter.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/status_or.h"
#include "google/cloud/storage/upload_options.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace noex {
/**
 * A Google Cloud Storage (GCS) Client for applications that disable exceptions.
 *
 * @see `google::cloud::storage::Client` for more details.
 */
class Client {
 public:
  /**
   * Creates the default client type with the default configuration.
   */
  explicit Client() : Client(ClientOptions()) {}

  /**
   * Creates the default client type given the options.
   *
   * @param options the client options, these are used to control credentials,
   *   buffer sizes, etc.
   * @param policies the client policies, these control the behavior of the
   *   client, for example, how to backoff when a operation needs to be retried,
   *   or what operations cannot be retried because they are not idempotent.
   *
   * @par Idempotency Policy Example
   * @snippet storage_object_samples.cc insert object strict idempotency
   *
   * @par Modified Retry Policy Example
   * @snippet storage_object_samples.cc insert object modified retry
   */
  // TODO(#1694) - remove all the code duplicated in `storage::Client`.
  template <typename... Policies>
  explicit Client(ClientOptions options, Policies&&... policies)
      : Client(CreateDefaultClient(std::move(options)),
               std::forward<Policies>(policies)...) {}

  /**
   * Creates the default client type given the credentials and policies.
   */
  // TODO(#1694) - remove all the code duplicated in `storage::Client`.
  template <typename... Policies>
  explicit Client(std::shared_ptr<oauth2::Credentials> credentials,
                  Policies&&... policies)
      : Client(ClientOptions(std::move(credentials)),
               std::forward<Policies>(policies)...) {}

  /// Builds a client and maybe override the retry, idempotency, and/or backoff
  /// policies.
  // TODO(#1694) - remove all the code duplicated in `storage::Client`.
  template <typename... Policies>
  explicit Client(std::shared_ptr<internal::RawClient> client,
                  Policies&&... policies)
      : raw_client_(
            Decorate(std::move(client), std::forward<Policies>(policies)...)) {}

  /// A tag to indicate the constructors should not decorate any RawClient.
  struct NoDecorations {};

  /// Builds a client with an specific RawClient, without decorations.
  // TODO(#1694) - remove all the code duplicated in `storage::Client`.
  explicit Client(std::shared_ptr<internal::RawClient> client, NoDecorations)
      : raw_client_(std::move(client)) {}

  std::shared_ptr<internal::RawClient> raw_client() const {
    return raw_client_;
  }

  //@{
  /**
   * @name Pub/Sub operations.
   *
   * Cloud Pub/Sub Notifications sends information about changes to objects in
   * your buckets to Cloud Pub/Sub, where the information is added to a Cloud
   * Pub/Sub topic of your choice in the form of messages.
   *
   * @see https://cloud.google.com/storage/docs/pubsub-notifications for more
   *     information about Cloud Pub/Sub in the context of GCS.
   */
  /**
   * Retrieves the list of Notifications for a Bucket.
   *
   * Cloud Pub/Sub Notifications sends information about changes to objects
   * in your buckets to Google Cloud Pub/Sub service.
   *
   * @param bucket_name the name of the bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_notification_samples.cc list notifications
   */
  template <typename... Options>
  StatusOr<std::vector<NotificationMetadata>> ListNotifications(
      std::string const& bucket_name, Options&&... options) {
    internal::ListNotificationsRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto result = raw_client_->ListNotifications(request);
    if (not result.first.ok()) {
      return std::move(result.first);
    }
    return std::move(result.second.items);
  }

  /**
   * Creates a new notification config for a Bucket.
   *
   * Cloud Pub/Sub Notifications sends information about changes to objects
   * in your buckets to Google Cloud Pub/Sub service. You can create multiple
   * notifications per Bucket, with different topics and filtering options.
   *
   * @param bucket_name the name of the bucket.
   * @param topic_name the Google Cloud Pub/Sub topic that will receive the
   *     notifications. This requires the full name of the topic, i.e.:
   *     `projects/<PROJECT_ID>/topics/<TOPIC_ID>`.
   * @param payload_format how will the data be formatted in the notifications,
   *     consider using the helpers in the `payload_format` namespace, or
   *     specify one of the valid formats defined in:
   *     https://cloud.google.com/storage/docs/json_api/v1/notifications
   * @param metadata define any optional parameters for the notification, such
   *     as the list of event types, or any custom attributes.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_notification_samples.cc create notification
   *
   * @see https://cloud.google.com/storage/docs/pubsub-notifications for general
   *     information on Cloud Pub/Sub Notifications for Google Cloud Storage.
   *
   * @see https://cloud.google.com/pubsub/ for general information on Google
   *     Cloud Pub/Sub service.
   */
  template <typename... Options>
  StatusOr<NotificationMetadata> CreateNotification(
      std::string const& bucket_name, std::string const& topic_name,
      std::string const& payload_format, NotificationMetadata metadata,
      Options&&... options) {
    metadata.set_topic(topic_name).set_payload_format(payload_format);
    internal::CreateNotificationRequest request(bucket_name, metadata);
    request.set_multiple_options(std::forward<Options>(options)...);
    return AsStatusOr(raw_client()->CreateNotification(request));
  }

  /**
   * Gets the details about a notification config in a given Bucket.
   *
   * Cloud Pub/Sub Notifications sends information about changes to objects
   * in your buckets to Google Cloud Pub/Sub service. You can create multiple
   * notifications per Bucket, with different topics and filtering options. This
   * function fetches the detailed information for a given notification config.
   *
   * @param bucket_name the name of the bucket.
   * @param notification_id the id of the notification config.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_notification_samples.cc get notification
   *
   * @see https://cloud.google.com/storage/docs/pubsub-notifications for general
   *     information on Cloud Pub/Sub Notifications for Google Cloud Storage.
   *
   * @see https://cloud.google.com/pubsub/ for general information on Google
   *     Cloud Pub/Sub service.
   */
  template <typename... Options>
  StatusOr<NotificationMetadata> GetNotification(
      std::string const& bucket_name, std::string const& notification_id,
      Options&&... options) {
    internal::GetNotificationRequest request(bucket_name, notification_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    return AsStatusOr(raw_client()->GetNotification(request));
  }

  /**
   * Delete an existing notification config in a given Bucket.
   *
   * Cloud Pub/Sub Notifications sends information about changes to objects
   * in your buckets to Google Cloud Pub/Sub service. You can create multiple
   * notifications per Bucket, with different topics and filtering options. This
   * function deletes one of the notification configs.
   *
   * @param bucket_name the name of the bucket.
   * @param notification_id the id of the notification config.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is always idempotent because it only acts on a specific
   * `notification_id`, the state after calling this function multiple times is
   * to delete that notification.  New notifications get different ids.
   *
   * @par Example
   * @snippet storage_notification_samples.cc delete notification
   *
   * @see https://cloud.google.com/storage/docs/pubsub-notifications for general
   *     information on Cloud Pub/Sub Notifications for Google Cloud Storage.
   *
   * @see https://cloud.google.com/pubsub/ for general information on Google
   *     Cloud Pub/Sub service.
   */
  template <typename... Options>
  StatusOr<void> DeleteNotification(std::string const& bucket_name,
                                    std::string const& notification_id,
                                    Options&&... options) {
    internal::DeleteNotificationRequest request(bucket_name, notification_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    return AsStatusOr(raw_client()->DeleteNotification(request));
  }
  //@}

 private:
  template <typename T>
  StatusOr<T> AsStatusOr(std::pair<Status, T> result) {
    if (not result.first.ok()) {
      return result.first;
    }
    return result.second;
  }

  StatusOr<void> AsStatusOr(std::pair<Status, internal::EmptyResponse> result) {
    return result.first;
  }

  // TODO(#1694) - remove all the code duplicated in `storage::Client`.
  static std::shared_ptr<internal::RawClient> CreateDefaultClient(
      ClientOptions options);

  template <typename... Policies>
  std::shared_ptr<internal::RawClient> Decorate(
      std::shared_ptr<internal::RawClient> client, Policies&&... policies) {
    // TODO(#1694) - remove all the code duplicated in `storage::Client`.
    auto logging = std::make_shared<internal::LoggingClient>(std::move(client));
    auto retry = std::make_shared<internal::RetryClient>(
        std::move(logging), internal::RetryClient::NoexPolicy{},
        std::forward<Policies>(policies)...);
    return retry;
  }

  std::shared_ptr<internal::RawClient> raw_client_;
};

}  // namespace noex
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_NOEX_CLIENT_H_
