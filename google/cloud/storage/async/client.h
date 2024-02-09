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
#include "google/cloud/storage/async/rewriter.h"
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
   * @par Selecting an upload function
   *
   * When choosing an upload method consider the following tradeoffs:
   *
   * @parblock
   * We recommend using `InsertObject()` for relatively small objects that fit
   * in memory.
   *
   * - *Pro:* Easy to use, a single function call uploads the object.
   * - *Pro:* Lowest latency for small objects. Use <= 4MiB as a rule of thumb.
   *   The precise threshold depends on your environment.
   * - *Con:* Recovery from transient errors requires resending all the data.
   * - *Con:* Multiple concurrent calls to `InsertObject()` will consume as
   *   much memory as is needed to hold all the data.
   *
   * We recommend using `StartBufferedUpload()` to upload data of unknown or
   * arbitrary size.
   *
   * - *Pro:* Relatively easy to use, the library can automatically resend
   *   data under most transient errors.
   * - *Pro:* The application can limit the amount of memory used by each
   *   upload, even if the full object is arbitrarily large.
   * - *Pro:* Can be used to upload "streaming" data sources where it is
   *   inefficient or impossible to go back and re-read data from an arbitrary
   *   point.
   * - *Con:* Throughput is limited as it needs to periodically wait for the
   *   service to flush the buffer to persistent storage.
   * - *Con:* Cannot automatically resume uploads after the application
   *   restarts.
   *
   * We recommend using `StartUnbufferedUpload()` to upload data where the
   * upload can efficiently resume from arbitrary points.
   *
   * - *Pro:* Can achieve the maximum theoretical throughput for a single
   *   stream upload. It is possible to use [Parallel Composite Uploads] to
   *   achieve even higher throughput.
   * - *Pro:* It can resume uploads even after the application restarts.
   * - *Con:* Requires manually handling transient errors during the upload.
   *
   * [Parallel Composite Uploads]:
   * https://cloud.google.com/storage/docs/parallel-composite-uploads
   * @endparblock
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
    return connection_->InsertObject(
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
        ->ReadObject(
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
    return connection_->ReadObjectRange(
        {ReadObjectRequest(std::move(bucket_name), std::move(object_name))
             .set_multiple_options(std::forward<Args>(args)...,
                                   storage::ReadRange(offset, offset + limit)),
         std::move(options)});
  }

  /**
   * Uploads a new object without buffering.
   *
   * @parblock
   * We recommend using `InsertObject()` for relatively small objects that fit
   * in memory.
   *
   * - *Pro:* Easy to use, a single function call uploads the object.
   * - *Pro:* Lowest latency for small objects. Use <= 4MiB as a rule of thumb.
   *   The precise threshold depends on your environment.
   * - *Con:* Recovery from transient errors requires resending all the data.
   * - *Con:* Multiple concurrent calls to `InsertObject()` will consume as
   *   much memory as is needed to hold all the data.
   *
   * We recommend using `StartBufferedUpload()` to upload data of unknown or
   * arbitrary size.
   *
   * - *Pro:* Relatively easy to use, the library can automatically resend
   *   data under most transient errors.
   * - *Pro:* The application can limit the amount of memory used by each
   *   upload, even if the full object is arbitrarily large.
   * - *Pro:* Can be used to upload "streaming" data sources where it is
   *   inefficient or impossible to go back and re-read data from an arbitrary
   *   point.
   * - *Con:* Throughput is limited as it needs to periodically wait for the
   *   service to flush the buffer to persistent storage.
   * - *Con:* Cannot automatically resume uploads after the application
   *   restarts.
   *
   * We recommend using `StartUnbufferedUpload()` to upload data where the
   * upload can efficiently resume from arbitrary points.
   *
   * - *Pro:* Can achieve the maximum theoretical throughput for a single
   *   stream upload. It is possible to use [Parallel Composite Uploads] to
   *   achieve even higher throughput.
   * - *Pro:* It can resume uploads even after the application restarts.
   * - *Con:* Requires manually handling transient errors during the upload.
   *
   * [Parallel Composite Uploads]:
   * https://cloud.google.com/storage/docs/parallel-composite-uploads
   * @endparblock
   *
   * This function always uses [resumable uploads][resumable-link]. The
   * application can provide a `#RestoreResumableUploadSession()` option to
   * resume a previously created upload. The returned object has accessors to
   * query the session ID and the amount of data persisted by the service.
   *
   * When resuming uploads it is the application's responsibility to save
   * the session ID to restart the upload later. Likewise, it is the
   * application's responsibility to query the amount of data already persisted
   * and to send the remaining data **without gaps or duplications**.
   *
   * If the application does not provide a `#RestoreResumableUploadSession()`
   * option, or it provides the `#NewResumableUploadSession()` option, then this
   * function will create a new resumable upload session.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param args a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `ContentEncoding`, `ContentType`,
   *   `Crc32cChecksumValue`, `DisableCrc32cChecksum`, `DisableMD5Hash`,
   *   `EncryptionKey`, `IfGenerationMatch`, `IfGenerationNotMatch`,
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `KmsKeyName`,
   *   `MD5HashValue`, `PredefinedAcl`, `Projection`,
   *   `UseResumableUploadSession`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * The client library always retries (a) any RPCs to create a resumable upload
   * session, and (b) any RPCs to query the status of a resumable upload
   * session. The client library never retries functions to upload more data
   * or to finalize the upload. The caller can retry these functions if it is
   * safe to do so.
   *
   * @par Example
   * @snippet storage_async_samples.cc write-object
   *
   * @par Example
   * @snippet storage_async_samples.cc write-object-with-retry
   *
   * @see [Resumable Uploads][resumable-link] for more information about
   *     resumable uploads.
   *
   * [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
   * [service documentation]:
   * https://cloud.google.com/storage/docs/uploads-downloads#size
   */
  template <typename... Args>
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartUnbufferedUpload(
      std::string bucket_name, std::string object_name, Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_
        ->StartUnbufferedUpload(
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
   * Uploads a new object with buffering and automatic recovery from transient
   * failures.
   *
   * @parblock
   * We recommend using `InsertObject()` for relatively small objects that fit
   * in memory.
   *
   * - *Pro:* Easy to use, a single function call uploads the object.
   * - *Pro:* Lowest latency for small objects. Use <= 4MiB as a rule of thumb.
   *   The precise threshold depends on your environment.
   * - *Con:* Recovery from transient errors requires resending all the data.
   * - *Con:* Multiple concurrent calls to `InsertObject()` will consume as
   *   much memory as is needed to hold all the data.
   *
   * We recommend using `StartBufferedUpload()` to upload data of unknown or
   * arbitrary size.
   *
   * - *Pro:* Relatively easy to use, the library can automatically resend
   *   data under most transient errors.
   * - *Pro:* The application can limit the amount of memory used by each
   *   upload, even if the full object is arbitrarily large.
   * - *Pro:* Can be used to upload "streaming" data sources where it is
   *   inefficient or impossible to go back and re-read data from an arbitrary
   *   point.
   * - *Con:* Throughput is limited as it needs to periodically wait for the
   *   service to flush the buffer to persistent storage.
   * - *Con:* Cannot automatically resume uploads after the application
   *   restarts.
   *
   * We recommend using `StartUnbufferedUpload()` to upload data where the
   * upload can efficiently resume from arbitrary points.
   *
   * - *Pro:* Can achieve the maximum theoretical throughput for a single
   *   stream upload. It is possible to use [Parallel Composite Uploads] to
   *   achieve even higher throughput.
   * - *Pro:* It can resume uploads even after the application restarts.
   * - *Con:* Requires manually handling transient errors during the upload.
   *
   * [Parallel Composite Uploads]:
   * https://cloud.google.com/storage/docs/parallel-composite-uploads
   * @endparblock
   *
   * This function always uses [resumable uploads][resumable-link]. The
   * application can provide a `#RestoreResumableUploadSession()` option to
   * resume a previously created upload. The returned object has accessors to
   * query the session ID and the amount of data persisted by the service.
   *
   * When resuming uploads it is the application's responsibility to save
   * the session ID to restart the upload later. Likewise, it is the
   * application's responsibility to query the amount of data already persisted
   * and to send the remaining data **without gaps or duplications**.
   *
   * If the application does not provide a `#RestoreResumableUploadSession()`
   * option, or it provides the `#NewResumableUploadSession()` option, then this
   * function will create a new resumable upload session.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param args a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `ContentEncoding`, `ContentType`,
   *   `Crc32cChecksumValue`, `DisableCrc32cChecksum`, `DisableMD5Hash`,
   *   `EncryptionKey`, `IfGenerationMatch`, `IfGenerationNotMatch`,
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `KmsKeyName`,
   *   `MD5HashValue`, `PredefinedAcl`, `Projection`,
   *   `UseResumableUploadSession`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * The client library always retries (a) any RPCs to create a resumable upload
   * session, and (b) any RPCs to query the status of a resumable upload
   * session.
   *
   * @see [Resumable Uploads][resumable-link] for more information about
   *     resumable uploads.
   *
   * [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
   * [service documentation]:
   * https://cloud.google.com/storage/docs/uploads-downloads#size
   */
  template <typename... Args>
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartBufferedUpload(
      std::string bucket_name, std::string object_name, Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    return connection_
        ->StartBufferedUpload(
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
    return connection_->ComposeObject(
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
    return connection_->DeleteObject(
        {DeleteObjectRequest(std::move(bucket_name), std::move(object_name))
             .set_multiple_options(std::forward<Args>(args)...),
         /*.options=*/std::move(options)});
  }

  /**
   * Creates an `AsyncRewriter` to copy the source object.
   *
   * Applications use this function to reliably copy objects across [location
   * boundaries](https://cloud.google.com/storage/docs/locations), and to
   * rewrite objects with different encryption keys. The operation returns an
   * `AsyncRewriter`, which the application can use to initiate the copy and to
   * iterate if the copy requires more than one call to complete.
   *
   * @note Application developers should be aware that rewriting large objects
   *     may take many calls to `AsyncRewriter::Iterate()`. Simple experiments
   *     rewriting an object within the same bucket or rewriting small objects
   *     often complete with a single call. For more information, see the
   *     [Object: rewrite] documentation.
   *
   * @param source_bucket_name the name of the bucket containing the source
   *     object.
   * @param source_object_name the name of the source object.
   * @param destination_bucket_name where the destination object will be
   *     located.
   * @param destination_object_name what to name the destination object.
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationKmsKeyName`,
   *      `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *      `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *      `IfSourceGenerationMatch`, `IfSourceGenerationNotMatch`,
   *      `IfSourceMetagenerationMatch`, `IfSourceMetagenerationNotMatch`,
   *      `MaxBytesRewrittenPerCall`, `Projection`, `SourceEncryptionKey`,
   *      `SourceGeneration`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is purely local, and always succeeds.  The `Iterate()` calls
   * might fail and are only idempotent if restricted by pre-conditions. In this
   * call the most relevant pre-condition is `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_async_samples.cc rewrite-object
   *
   * @see
   * [Object: rewrite]:
   * https://cloud.google.com/storage/docs/json_api/v1/objects/rewrite
   */
  template <typename... Args>
  std::pair<AsyncRewriter, AsyncToken> StartRewrite(
      std::string source_bucket_name, std::string source_object_name,
      std::string destination_bucket_name, std::string destination_object_name,
      Args&&... args) {
    return ResumeRewrite(
        std::move(source_bucket_name), std::move(source_object_name),
        std::move(destination_bucket_name), std::move(destination_object_name),
        /*rewrite_token=*/"", std::forward<Args>(args)...);
  }

  /**
   * Creates an `AsyncRewriter` to resume copying the source object.
   *
   * Applications use this function to reliably copy objects across [location
   * boundaries](https://cloud.google.com/storage/docs/locations), and to
   * rewrite objects with different encryption keys. The operation returns an
   * `AsyncRewriter`, which the application can use to initiate the copy and to
   * iterate if the copy requires more than one call to complete.
   *
   * @note Application developers should be aware that rewriting large objects
   *     may take many calls to `AsyncRewriter::Iterate()`. Simple experiments
   *     rewriting an object within the same bucket or rewriting small objects
   *     often complete with a single call. For more information, see the
   *     [Object: rewrite] documentation.
   *
   * @param source_bucket_name the name of the bucket containing the source
   *     object.
   * @param source_object_name the name of the source object.
   * @param destination_bucket_name where the destination object will be
   *     located.
   * @param destination_object_name what to name the destination object.
   * @param rewrite_token the token from a previous successful rewrite
   *     iteration. Can be the empty string, in which case this starts a new
   *     rewrite operation.
   * @param args a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationKmsKeyName`,
   *      `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *      `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *      `IfSourceGenerationMatch`, `IfSourceGenerationNotMatch`,
   *      `IfSourceMetagenerationMatch`, `IfSourceMetagenerationNotMatch`,
   *      `MaxBytesRewrittenPerCall`, `Projection`, `SourceEncryptionKey`,
   *      `SourceGeneration`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is purely local, and always succeeds.  The `Iterate()` calls
   * might fail and are only idempotent if restricted by pre-conditions. In this
   * call the most relevant pre-condition is `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_async_samples.cc resume-rewrite
   *
   * @see
   * [Object: rewrite]:
   * https://cloud.google.com/storage/docs/json_api/v1/objects/rewrite
   */
  template <typename... Args>
  std::pair<AsyncRewriter, AsyncToken> ResumeRewrite(
      std::string source_bucket_name, std::string source_object_name,
      std::string destination_bucket_name, std::string destination_object_name,
      std::string rewrite_token, Args&&... args) {
    auto options = SpanOptions(std::forward<Args>(args)...);
    auto c = connection_->RewriteObject(
        {RewriteObjectRequest(std::move(source_bucket_name),
                              std::move(source_object_name),
                              std::move(destination_bucket_name),
                              std::move(destination_object_name))
             .set_rewrite_token(std::move(rewrite_token))
             .set_multiple_options(std::forward<Args>(args)...),
         /*.options=*/std::move(options)});
    auto t = storage_internal::MakeAsyncToken(c.get());
    return std::make_pair(AsyncRewriter(std::move(c)), std::move(t));
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
