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

 private:
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
