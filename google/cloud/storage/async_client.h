// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CLIENT_H

#include "google/cloud/storage/internal/async_connection.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A client for Google Cloud Storage offering asynchronous operations.
 */
class AsyncClient {
 public:
  ~AsyncClient() = default;

  /**
   * Deletes an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if:
   * - restricted by pre-conditions, in this case, `IfGenerationMatch`
   * - or, if it applies to only one object version via `Generation`.
   */
  template <typename... RequestOptions>
  future<Status> DeleteObject(std::string const& bucket_name,
                              std::string const& object_name,
                              RequestOptions&&... options) {
    internal::OptionsSpan span(connection_->options());
    storage::internal::DeleteObjectRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<RequestOptions>(options)...);
    return connection_->AsyncDeleteObject(request);
  }

 private:
  friend AsyncClient MakeAsyncClient(Options opts);
  explicit AsyncClient(
      std::shared_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<storage_internal::AsyncConnection> connection);

  std::shared_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<storage_internal::AsyncConnection> connection_;
};

// TODO(#7142) - expose a factory function / constructor consuming
//     std::shared_ptr<AsyncConnection> when we have a plan for mocking
/// Creates a new GCS client exposing asynchronous APIs.
AsyncClient MakeAsyncClient(Options opts = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CLIENT_H
