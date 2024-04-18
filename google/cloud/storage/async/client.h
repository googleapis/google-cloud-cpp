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

#include "google/cloud/storage/async/bucket_name.h"
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
#include <google/storage/v2/storage.pb.h>

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
 * auto pending = client.DeleteObject("my-bucket", "my-object");
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
 * auto pending = client.DeleteObject(
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

  /*
  This snippet discusses the tradeoffs between `InsertObject()`,
  `StartBufferedUpload()`, and `StartUnbufferedUpload()`. The text is included
  in the Doxygen documentation for these function too.

  [selecting-an-upload-function]
  @par Selecting an upload function
  @parblock
  When choosing an upload method consider the following tradeoffs:

  We recommend using `InsertObject()` for relatively small objects that fit in
  memory.

  - *Pro:* Easy to use, a single function call uploads the object.
  - *Pro:* Lowest latency for small objects. Use <= 4MiB as a rule of thumb.
    The precise threshold depends on your environment.
  - *Con:* Recovery from transient errors requires resending all the data.
  - *Con:* Multiple concurrent calls to `InsertObject()` will consume as
    much memory as is needed to hold all the data.

  We recommend using `StartBufferedUpload()` to upload data of unknown or
  arbitrary size.

  - *Pro:* Relatively easy to use, the library can automatically resend data
    under most transient errors.
  - *Pro:* The application can limit the amount of memory used by each upload,
    even if the full object is arbitrarily large.
  - *Pro:* Can be used to upload "streaming" data sources where it is
    inefficient or impossible to go back and re-read data from an arbitrary
    point.
  - *Con:* Throughput is limited as it needs to periodically wait for the
    service to flush the buffer to persistent storage.
  - *Con:* Cannot automatically resume uploads after the application restarts.

  We recommend using `StartUnbufferedUpload()` to upload data where the upload
  can efficiently resume from arbitrary points.

  - *Pro:* Can achieve the maximum theoretical throughput for a single stream
    upload. It is possible to use [Parallel Composite Uploads] to achieve even
    higher throughput.
  - *Pro:* It can resume uploads even after the application restarts.
  - *Con:* Requires manually handling transient errors during the upload.

  [Parallel Composite Uploads]:
  https://cloud.google.com/storage/docs/parallel-composite-uploads
  @endparblock
  [selecting-an-upload-function]
  */

  /**
   * Creates an object given its name and contents.
   *
   * This function always uses [single-request uploads][single-request-link].
   * As the name implies, these uploads use a single RPC to upload all the data.
   * There is no way to restart or resume these uploads if there is a partial
   * failure. All the data must be sent again in that case.
   *
   * @snippet{doc} async/client.h selecting-an-upload-function
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
   *
   * [single-request-link]:
   * https://cloud.google.com/storage/docs/uploads-downloads#uploads
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

  /*
  [start-buffered-upload-common]
  This function always uses [resumable uploads][resumable-link]. The objects
  returned by this function buffer data until it is persisted on the service.
  If the buffer becomes full, they stop accepting new data until the service
  has persisted enough data.

  Because these object buffer data they can recover from most transient
  errors, including an unexpected closure of the streaming RPC used for the
  upload. The downside is that these objects must periodically flush these
  buffers, and this may not achieve the highest possible throughput.

  @par Example
  @snippet storage_async_samples.cc start-buffered-upload

  @par Example
  @snippet storage_async_samples.cc suspend-buffered-upload

  @par Idempotency
  This function is always treated as idempotent, and the library will
  automatically retry the function on transient errors. Note that this may
  create multiple upload ids. This is safe as any additional upload ids have no
  cost and are not visible to any application.

  @snippet{doc} async/client.h selecting-an-upload-function

  [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
  [start-buffered-upload-common]
  */

  /**
   * Starts a new resumable upload session with client-side buffering and
   * automatic recovery from transient failures.
   *
   * @snippet{doc} async/client.h start-buffered-upload-common
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartBufferedUpload(
      BucketName const& bucket_name, std::string object_name,
      Options opts = {});

  /**
   * Starts a new resumable upload session with client-side buffering and
   * automatic recovery from transient failures.
   *
   * @snippet{doc} async/client.h start-buffered-upload-common
   *
   * @param request the request contents, it must include the bucket name and
   *     object names. Many other fields are optional.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartBufferedUpload(
      google::storage::v2::StartResumableWriteRequest request,
      Options opts = {});

  /*
  [resume-buffered-common]
  Use this function to resume an upload after your application stops uploading
  data, even after your application starts.

  This function always uses [resumable uploads][resumable-link]. The objects
  returned by this function buffer data until it is persisted on the service.
  If the buffer becomes full, they stop accepting new data until the service
  has persisted enough data.

  @note It is the application's responsibility to check
      `#AsyncWriter::PersistedState()` and determine (1) if the upload was
      finalized, and if not finalized (2) the last persisted byte. The
      application **MUST** send data starting from that byte.

  @snippet{doc} async/client.h selecting-an-upload-function

  @par Example
  @parblock
  First use this example to partially upload an object:
  @snippet storage_async_samples.cc suspend-buffered-upload

  Then continue the upload using:
  @snippet storage_async_samples.cc resume-buffered-upload
  @endparblock

  @par Idempotency
  The client library always retries the RPCs used to resume uploads. These are
  read-only operations and therefore inherently idempotent.

  [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
  [resume-buffered-common]
  */

  /**
   * Resumes an object upload that automatically resumes on failures.
   *
   * @snippet{doc} async/client.h resume-buffered-common
   *
   * @param upload_id the id of the upload that should be resumed.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> ResumeBufferedUpload(
      std::string upload_id, Options opts = {});

  /**
   * Resumes an object upload that automatically resumes on failures.
   *
   * @snippet{doc} async/client.h resume-buffered-common
   *
   * @param request the full request to resume the upload. Must include the
   *     upload id.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> ResumeBufferedUpload(
      google::storage::v2::QueryWriteStatusRequest request, Options opts = {});

  /*
  [start-unbuffered-upload-common]
  This function always uses [resumable uploads][resumable-link]. The objects
  returned by this function do not buffer data and, therefore, cannot
  automatically recover from transient failures. On the other hand, they do not
  need to periodically flush any buffers, so they can achieve maximum throughput
  for a single upload stream.

  Use `#AsyncWriter::UploadId()` to save the upload id if you are planning to
  resume the upload.

  @snippet{doc} async/client.h selecting-an-upload-function

  @par Example
  @snippet storage_async_samples.cc start-unbuffered-upload

  @par Example
  @snippet storage_async_samples.cc suspend-unbuffered-upload

  @par Idempotency
  This function is always treated as idempotent, and the library will
  automatically retry the function on transient errors. Note that this may
  create multiple upload ids. This is safe as any additional upload ids have no
  cost and are not visible to any application.

  [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
  [start-unbuffered-upload-common]
  */

  /**
   * Starts a new resumable upload session without client-side buffering.
   *
   * @snippet{doc} async/client.h start-unbuffered-upload-common
   *
   * @param bucket_name the name of the bucket that will contain the object.
   * @param object_name the name of the object to be read.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartUnbufferedUpload(
      BucketName const& bucket_name, std::string object_name,
      Options opts = {});

  /**
   * Starts a new resumable upload session without client-side buffering.
   *
   * @snippet{doc} async/client.h start-unbuffered-upload-common
   *
   * @param request the request contents, it must include the bucket name and
   *     object names. Many other fields are optional.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> StartUnbufferedUpload(
      google::storage::v2::StartResumableWriteRequest request,
      Options opts = {});

  /*
  [resume-unbuffered-common]
  Use this function to resume an upload after your application stops uploading
  data, even after your application starts.

  This function always uses [resumable uploads][resumable-link]. The objects
  returned by this function buffer data until it is persisted on the service.
  If the buffer becomes full, they stop accepting new data until the service
  has persisted enough data.

  @note It is the application's responsibility to check
      `#AsyncWriter::PersistedState()` and determine (1) if the upload was
      finalized, and if not finalized (2) the last persisted byte. The
      application **MUST** send data starting from that byte.

  @snippet{doc} async/client.h selecting-an-upload-function

  @par Example
  @parblock
  First use this example to partially upload an object:
  @snippet storage_async_samples.cc suspend-unbuffered-upload

  Then continue the upload using:
  @snippet storage_async_samples.cc resume-unbuffered-upload
  @endparblock

  @par Idempotency
  The client library always retries the RPCs used to resume uploads. These are
  read-only operations and therefore inherently idempotent.

  [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
  [resume-unbuffered-common]
  */
  /**
   * Resumes an upload without buffering or automatic recovery from transient
   * failures.
   *
   * @snippet{doc} async/client.h resume-unbuffered-common
   *
   * @param upload_id the id of the upload that should be resumed.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> ResumeUnbufferedUpload(
      std::string upload_id, Options opts = {});

  /**
   * Resumes an upload without buffering or automatic recovery from transient
   * failures.
   *
   * @snippet{doc} async/client.h resume-unbuffered-common
   *
   * @param request the full request to resume the upload. Must include the
   *     upload id.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<std::pair<AsyncWriter, AsyncToken>>> ResumeUnbufferedUpload(
      google::storage::v2::QueryWriteStatusRequest request, Options opts = {});

  /**
   * Composes existing objects into a new object in the same bucket.
   *
   * @par Example
   * @snippet storage_async_samples.cc compose-object
   *
   * @par Idempotency
   * This operation is never idempotent. Use the overload consuming a
   * `google::storage::v2::ComposeObjectRequest` and set pre-conditions on the
   * destination object to make the request idempotent.
   *
   * @param bucket_name the name of the bucket used for source object and
   *     destination objects.
   * @param destination_object_name the composed object name.
   * @param source_objects objects used to compose `destination_object_name`.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      BucketName const& bucket_name, std::string destination_object_name,
      std::vector<google::storage::v2::ComposeObjectRequest::SourceObject>
          source_objects,
      Options opts = {});

  /**
   * Composes existing objects into a new object in the same bucket.
   *
   * @par Example
   * @snippet storage_async_samples.cc compose-object-request
   *
   * @par Idempotency
   * This operation is idempotent if there are pre-conditions on the destination
   * object. Set the `if_generation_match()` or `if_metageneration_match()`
   * fields.
   *
   * @param request the full request describing what objects to compose, the
   *     name of the destination object, and any metadata for this destination
   *     object. See the proto documentation for details.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      google::storage::v2::ComposeObjectRequest request, Options opts = {});

  /*
  [delete-object-common]
  @par Example
  @snippet storage_async_samples.cc delete-object

  @par Idempotency
  This operation is only idempotent if:
  - restricted by pre-conditions, in this case, `IfGenerationMatch`
  - or, if it applies to only one object version via `Generation`.
  [delete-object-common]
  */
  /**
   * @brief Deletes an object
   *
   * @snippet{doc} async/client.h delete-object-common
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to delete.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<Status> DeleteObject(BucketName const& bucket_name,
                              std::string object_name, Options opts = {});

  /**
   * @brief Deletes an object
   *
   * @snippet{doc} async/client.h delete-object-common
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to delete.
   * @param generation the object generation to delete.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<Status> DeleteObject(BucketName const& bucket_name,
                              std::string object_name, std::int64_t generation,
                              Options opts = {});

  /**
   * @brief Deletes an object
   *
   * @snippet{doc} async/client.h delete-object-common
   *
   * @param request the full request describing what object to delete. It may
   *     also include any preconditions the object must satisfy, and other
   *     parameters that are necessary to complete the RPC.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  future<Status> DeleteObject(google::storage::v2::DeleteObjectRequest request,
                              Options opts = {});

  /*
  [start-rewrite-common]
  Applications use this function to reliably copy objects across [location
  boundaries](https://cloud.google.com/storage/docs/locations), and to rewrite
  objects with different encryption keys. The operation returns an
  `AsyncRewriter`, which the application can use to initiate the copy and to
  iterate if the copy requires more than one call to complete.

  @par Example
  @snippet storage_async_samples.cc rewrite-object

  @note Application developers should be aware that rewriting large objects
      may take many calls to `AsyncRewriter::Iterate()`. Simple experiments
      often complete with a single call because they use small objects, or
      use objects where rewrites do not require moving data. For more
      information, see the [Object: rewrite] documentation.

  @par Idempotency
  This operation is purely local, and always succeeds. The `Iterate()` calls
  are always treated as idempotent. Their only observable side effect is the
  creation of the object, and this can only succeed once.

  [Object: rewrite]:
  https://cloud.google.com/storage/docs/json_api/v1/objects/rewrite
  [start-rewrite-common]
  */

  /**
   * Creates an `AsyncRewriter` to copy the source object.
   *
   * @snippet{doc} async/client.h start-rewrite-common
   *
   * @param source_bucket the name of the bucket containing the source object.
   * @param source_object_name the name of the source object.
   * @param destination_bucket the name of the bucket for the new object.
   * @param destination_object_name what to name the destination object.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  std::pair<AsyncRewriter, AsyncToken> StartRewrite(
      BucketName const& source_bucket, std::string source_object_name,
      BucketName const& destination_bucket, std::string destination_object_name,
      Options opts = {});

  /**
   * Creates an `AsyncRewriter` to copy the source object.
   *
   * @snippet{doc} async/client.h start-rewrite-common
   *
   * @param request any additional parameters modifying the request, such as
   *   pre-conditions, and overrides for the destination object metadata.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  std::pair<AsyncRewriter, AsyncToken> StartRewrite(
      google::storage::v2::RewriteObjectRequest request, Options opts = {});

  /*
     [resume-rewrite-common]
     Applications use this function to reliably copy objects across [location
     boundaries](https://cloud.google.com/storage/docs/locations), and to
     rewrite objects with different encryption keys. The operation returns an
     `AsyncRewriter`, which the application can use to continue an existing copy
     operation until it completes.

     @par Example
     @snippet storage_async_samples.cc resume-rewrite

     @note Application developers should be aware that rewriting large objects
         may take many calls to `AsyncRewriter::Iterate()`. Simple experiments
         often complete with a single call because they use small objects, or
         use objects where rewrites do not require moving data. For more
         information, see the [Object: rewrite] documentation.

     @par Idempotency
     This operation is purely local, and always succeeds. The `Iterate()` calls
     are always treated as idempotent. Their only observable side effect is the
     creation of the object, and this can only succeed once.

     [Object: rewrite]:
     https://cloud.google.com/storage/docs/json_api/v1/objects/rewrite
     [resume-rewrite-common]
   */

  /**
   * Creates an `AsyncRewriter` to resume copying the source object.
   *
   * @snippet{doc} async/client.h resume-rewrite-common
   *
   * @param source_bucket the name of the bucket containing the source object.
   * @param source_object_name the name of the source object.
   * @param destination_bucket the name of the bucket for the new object.
   * @param destination_object_name what to name the destination object.
   * @param rewrite_token the token from a previous successful rewrite
   *     iteration. Can be the empty string, in which case this starts a new
   *     rewrite operation.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   *
   * @par Example
   * @snippet storage_async_samples.cc resume-rewrite
   */
  std::pair<AsyncRewriter, AsyncToken> ResumeRewrite(
      BucketName const& source_bucket, std::string source_object_name,
      BucketName const& destination_bucket, std::string destination_object_name,
      std::string rewrite_token, Options opts = {});

  /**
   * Creates an `AsyncRewriter` to resume copying the source object.
   *
   * @snippet{doc} async/client.h resume-rewrite-common
   *
   * @param request the full specification for the request.
   * @param opts options controlling the behavior of this RPC, for example
   *     the application may change the retry policy.
   */
  std::pair<AsyncRewriter, AsyncToken> ResumeRewrite(
      google::storage::v2::RewriteObjectRequest request, Options opts = {});

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
