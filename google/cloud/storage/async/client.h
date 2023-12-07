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

#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/async/reader.h"
#include "google/cloud/storage/async/token.h"
#include "google/cloud/storage/async/writer.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
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
  /// Create a new client configured with @p options.
  explicit AsyncClient(Options options = {});
  /// Create a new client using @p connection. This is often used for mocking.
  explicit AsyncClient(std::shared_ptr<AsyncConnection> connection);

  ~AsyncClient() = default;

  /**
   * Creates an object given its name and contents.
   *
   * @param bucket_name the name of the bucket that will contain the object.
   * @param object_name the name of the object to be created.
   * @param contents the contents (media) for the new object.
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `ContentEncoding`,
   *     `ContentType`, `Crc32cChecksumValue`, `DisableCrc32cChecksum`,
   *     `DisableMD5Hash`, `EncryptionKey`, `IfGenerationMatch`,
   *     `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `KmsKeyName`, `MD5HashValue`,
   *     `PredefinedAcl`, `Projection`, `UserProject`, and `WithObjectMetadata`.
   * @tparam Collection the type for the payload. This must be convertible to
   *   `std::string`, `std::vector<CharType>`, `std::vector<std::string>`, or
   *   `std::vector<std::vector<ChartType>>`. Where `ChartType` is a `char`,
   *   `signed char`, `unsigned char`, or `std::uint8_t`.
   * @tparam Args the types of any optional arguments.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_async_samples.cc insert-object
   */
  template <typename Collection, typename... Args>
  future<StatusOr<storage::ObjectMetadata>> InsertObject(
      std::string bucket_name, std::string object_name, Collection&& contents,
      Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_->AsyncInsertObject(
        {/*.request=*/InsertObjectRequest(std::move(bucket_name),
                                          std::move(object_name))
             .set_multiple_options(std::forward<Args>(args)...),
         /*.payload=*/
         WritePayload(std::forward<Collection>(contents)),
         /*.options=*/std::move(options)});
  }

  /**
   * Reads the contents of an object.
   *
   * When satisfied, the returned future has a reader to asynchronously download
   * the contents of the given object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DisableCrc32cChecksum`,
   *     `DisableMD5Hash`, `EncryptionKey`, `Generation`, `IfGenerationMatch`,
   *     `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `UserProject`, and `AcceptEncoding`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_async_samples.cc read-object
   */
  template <typename... Args>
  future<StatusOr<std::pair<AsyncReader, AsyncToken>>> ReadObject(
      std::string bucket_name, std::string object_name, Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_
        ->AsyncReadObject(
            {ReadObjectRequest(std::move(bucket_name), std::move(object_name))
                 .set_multiple_options(std::forward<Args>(args)...),
             std::move(options)})
        .then([](auto f) -> StatusOr<std::pair<AsyncReader, AsyncToken>> {
          auto impl = f.get();
          if (!impl) return std::move(impl).status();
          auto t = storage_internal::MakeAsyncToken(impl->get());
          return std::make_pair(AsyncReader(*std::move(impl)), std::move(t));
        });
  }

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
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DisableCrc32cChecksum`,
   *     `DisableMD5Hash`, `EncryptionKey`, `Generation`, `IfGenerationMatch`,
   *     `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `UserProject`, and `AcceptEncoding`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   */
  template <typename... Args>
  future<StatusOr<ReadPayload>> ReadObjectRange(std::string bucket_name,
                                                std::string object_name,
                                                std::int64_t offset,
                                                std::int64_t limit,
                                                Args&&... args) {
    struct HasReadRange
        : public absl::disjunction<
              std::is_same<storage::ReadRange, std::decay_t<Args>>...> {};
    struct HasReadFromOffset
        : public absl::disjunction<
              std::is_same<storage::ReadFromOffset, std::decay_t<Args>>...> {};
    struct HasReadLast
        : public absl::disjunction<
              std::is_same<storage::ReadLast, std::decay_t<Args>>...> {};

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

    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_->AsyncReadObjectRange(
        {ReadObjectRequest(std::move(bucket_name), std::move(object_name))
             .set_multiple_options(std::forward<Args>(args)...,
                                   storage::ReadRange(offset, offset + limit)),
         std::move(options)});
  }

  template <typename... Args>
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> WriteObject(
      std::string bucket_name, std::string object_name, Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_
        ->AsyncWriteObject(
            {ResumableUploadRequest(std::move(bucket_name),
                                    std::move(object_name))
                 .set_multiple_options(std::forward<Args>(args)...),
             std::move(options)})
        .then([](auto f) -> StatusOr<std::pair<AsyncWriter, AsyncToken>> {
          auto impl = f.get();
          if (!impl) return std::move(impl).status();
          auto t = absl::holds_alternative<storage::ObjectMetadata>(
                       (*impl)->PersistedState())
                       ? AsyncToken()
                       : storage_internal::MakeAsyncToken(impl->get());
          return std::make_pair(AsyncWriter(*std::move(impl)), std::move(t));
        });
  }

  /**
   * Composes existing objects into a new object in the same bucket.
   *
   * @param bucket_name the name of the bucket used for source object and
   *     destination object.
   * @param source_objects objects used to compose `destination_object_name`.
   * @param destination_object_name the composed object name.
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include
   *     `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *     `IfMetagenerationMatch`, `KmsKeyName`, `UserProject`, and
   *     `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   */
  template <typename... Args>
  future<StatusOr<storage::ObjectMetadata>> ComposeObject(
      std::string bucket_name,
      std::vector<storage::ComposeSourceObject> source_objects,
      std::string destination_object_name, Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_->AsyncComposeObject(
        {ComposeObjectRequest(std::move(bucket_name), std::move(source_objects),
                              std::move(destination_object_name))
             .set_multiple_options(std::forward<Args>(args)...),
         std::move(options)});
  }

  /**
   * Deletes an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, and `UserProject`.
   *     See the class description for common request options.
   *
   * @par Idempotency
   * This operation is only idempotent if:
   * - restricted by pre-conditions, in this case, `IfGenerationMatch`
   * - or, if it applies to only one object version via `Generation`.
   *
   * @par Example
   * @snippet storage_async_samples.cc delete-object
   */
  template <typename... Args>
  future<Status> DeleteObject(std::string bucket_name, std::string object_name,
                              Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_->AsyncDeleteObject(
        {DeleteObjectRequest(std::move(bucket_name), std::move(object_name))
             .set_multiple_options(std::forward<Args>(args)...),
         /*.options=*/std::move(options)});
  }

 private:
  explicit AsyncClient(
      std::shared_ptr<google::cloud::BackgroundThreads> background,
      std::shared_ptr<AsyncConnection> connection);

  template <typename... RequestOptions>
  google::cloud::Options SpanOptions(RequestOptions&&... o) const {
    return google::cloud::internal::GroupOptions(
        connection_->options(), std::forward<RequestOptions>(o)...);
  }

  std::shared_ptr<google::cloud::BackgroundThreads> background_;
  std::shared_ptr<AsyncConnection> connection_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CLIENT_H
