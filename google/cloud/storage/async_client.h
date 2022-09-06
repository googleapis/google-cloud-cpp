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
#include "google/cloud/internal/group_options.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A client for Google Cloud Storage offering asynchronous operations.
 *
 * @note This class is experimental, it is subject to change without notice.
 *
 * @par Optional Request Options
 * Most of the member functions in this class can receive optional request
 * options. For example, the default when deleting an object is to delete the
 * latest version:
 *
 * @code
 * auto pending = gcs.DeleteObject("my-bucket", "my-object");
 * @endcode
 *
 * Some applications may want to delete a specific version. In this case just
 * provide the `Generation` request option:
 *
 * @code
 * auto pending = gcs.DeleteObject(
 *     "my-bucket", "my-object", gcs::Generation(generation));
 * @endcode
 *
 * Each function documents the types accepted as optional request options. These
 * parameters can be specified in any order. Specifying a request option that is
 * not applicable to a member function results in a compile-time error.
 *
 * All operations support the following common request options:
 *
 * - `Fields`: return a [partial response], which includes only the desired
 *   fields.
 * - `QuotaUser`: attribute the request to this specific label for quota
 *   purposes.
 * - `UserProject`: change the request costs (if applicable) to this GCP
 *   project.
 * - `CustomHeader`: include a custom header with the request. These are
 *   typically used for testing, though they are sometimes helpful if
 *   environments where HTTPS traffic is mediated by a proxy.
 * - `UserIp`: attribute the request to this specific IP address for quota
 *   purpose. Not recommended, prefer `QuotaUser` instead.
 *
 * [partial response]:
 * https://cloud.google.com/storage/docs/json_api#partial-response
 *
 * @par Per-operation Overrides
 *
 * In addition to the request options, which are passed on to the service to
 * modify the request, you can specify options that override the local behavior
 * of the library.  For example, you can override the local retry policy:
 *
 * @code
 * auto pending = gcs.DeleteObject(
 *     "my-bucket", "my-object",
 *      google::cloud::Options{}
 *          .set<gcs::RetryPolicyOption>(
 *              gcs::LimitedErrorCountRetryPolicy(5).clone()));
 * @endcode
 *
 * @par Retry, Backoff, and Idempotency Policies
 *
 * The library automatically retries requests that fail with transient errors,
 * and follows the [recommended practice][exponential-backoff] to backoff
 * between retries.
 *
 * The default policies are to continue retrying for up to 15 minutes, and to
 * use truncated (at 5 minutes) exponential backoff, doubling the maximum
 * backoff period between retries. Likewise, the idempotency policy is
 * configured to retry all operations.
 *
 * The application can override these policies when constructing objects of this
 * class. The documentation for the constructors shows examples of this in
 * action.
 *
 * [exponential-backoff]:
 * https://cloud.google.com/storage/docs/exponential-backoff
 */
class AsyncClient {
 public:
  ~AsyncClient() = default;

  /**
   * Reads the contents of an object.
   *
   * When satisfied, the returned future has the contents of the given object
   * between @p offset and @p offset + @p limit (exclusive).
   *
   * Be aware that this will accumulate all the bytes in memory, you need to
   * consider whether @p limit is too large for your deployment environment.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param offset where to begin reading from the object, results in an error
   *     if the offset is larger than the object
   * @param limit how much data to read starting at @p offset
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DisableCrc32cChecksum`,
   *     `DisableMD5Hash`, `EncryptionKey`, `Generation`, `IfGenerationMatch`,
   *     `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `UserProject`, and `AcceptEncoding`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   */
  template <typename... RequestOptions>
  future<AsyncReadObjectRangeResponse> ReadObject(
      std::string const& bucket_name, std::string const& object_name,
      std::int64_t offset, std::int64_t limit, RequestOptions&&... options) {
    struct HasReadRange
        : public absl::disjunction<
              std::is_same<storage::ReadRange, RequestOptions>...> {};
    struct HasReadFromOffset
        : public absl::disjunction<
              std::is_same<storage::ReadFromOffset, RequestOptions>...> {};
    struct HasReadLast
        : public absl::disjunction<
              std::is_same<storage::ReadLast, RequestOptions>...> {};

    static_assert(!HasReadRange::value,
                  "Cannot use `ReadRange()` as a request option in "
                  "`AsyncClient::ReadObject()`, use the `offset` and `limit` "
                  "parameters instead.");
    static_assert(!HasReadFromOffset::value,
                  "Cannot use `ReadFromOffset()` as a request option in "
                  "`AsyncClient::ReadObject()`, use the `offset` and `limit` "
                  "parameters instead.");
    static_assert(!HasReadLast::value,
                  "Cannot use `ReadLast()` as a request option in "
                  "`AsyncClient::ReadObject()`, use the `offset` and `limit` "
                  "parameters instead.");

    google::cloud::internal::OptionsSpan const span(
        SpanOptions(std::forward<Options>(options)...));
    storage::internal::ReadObjectRangeRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...,
                                 storage::ReadRange(offset, offset + limit));
    return connection_->AsyncReadObjectRange(std::move(request));
  }

  /**
   * Deletes an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, and `UserProject`.
   *     See the class description for common request options.
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
    google::cloud::internal::OptionsSpan const span(
        SpanOptions(std::forward<RequestOptions>(options)...));
    storage::internal::DeleteObjectRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<RequestOptions>(options)...);
    return connection_->AsyncDeleteObject(std::move(request));
  }

 private:
  friend AsyncClient MakeAsyncClient(Options opts);
  explicit AsyncClient(
      std::shared_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<storage_internal::AsyncConnection> connection);

  template <typename... RequestOptions>
  google::cloud::Options SpanOptions(RequestOptions&&... o) const {
    return google::cloud::internal::GroupOptions(
        connection_->options(), std::forward<RequestOptions>(o)...);
  }

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
