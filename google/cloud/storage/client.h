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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H

#include "google/cloud/storage/hmac_key_metadata.h"
#include "google/cloud/storage/internal/logging_client.h"
#include "google/cloud/storage/internal/parameter_pack_validation.h"
#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/internal/tuple_filter.h"
#include "google/cloud/storage/list_buckets_reader.h"
#include "google/cloud/storage/list_hmac_keys_reader.h"
#include "google/cloud/storage/list_objects_and_prefixes_reader.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/notification_event_type.h"
#include "google/cloud/storage/notification_payload_format.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/object_rewriter.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/upload_options.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/meta/type_traits.h"
#include <type_traits>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
class ClientTester;
}  // namespace testing
inline namespace STORAGE_CLIENT_NS {
namespace internal {
class NonResumableParallelUploadState;
class ResumableParallelUploadState;
}  // namespace internal
/**
 * The Google Cloud Storage (GCS) Client.
 *
 * This is the main class to interact with GCS. It provides member functions to
 * invoke all the APIs in the service.
 *
 * @par Performance
 * Creating an object of this type is a relatively low-cost operation.
 * Connections to the service are created on demand. Copy-assignment and
 * copy-construction are also relatively low-cost operations, they should be
 * comparable to copying a few shared pointers. The first request (or any
 * request that requires a new connection) incurs the cost of creating the
 * connection and authenticating with the service. Note that the library may
 * need to perform other bookeeping operations that may impact performance.
 * For example, access tokens need to be refreshed from time to time, and this
 * may impact the performance of some operations.
 *
 * @par Thread-safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Credentials
 * The default approach for creating a Client uses Google Application Default
 * %Credentials (ADCs). Because finding or loading ADCs can fail, the returned
 * `StatusOr<Client>` from `CreateDefaultClient()` should be verified before
 * using it. However, explicitly passing `Credentials` when creating a Client
 * does not have the same potential to fail, so the resulting `Client` is not
 * wrapped in a `StatusOr`.  If you wish to use `AnonymousCredentials` or to
 * supply a specific `Credentials` type, you can use the functions declared in
 * google_credentials.h:
 * @code
 * namespace gcs = google::cloud::storage;
 *
 * // Implicitly use ADCs:
 * StatusOr<gcs::Client> client = gcs::Client::CreateDefaultClient();
 * if (!client) {
 *   // Handle failure and return.
 * }
 *
 * // Or explicitly use ADCs:
 * auto creds = gcs::oauth2::GoogleDefaultCredentials();
 * if (!creds) {
 *   // Handle failure and return.
 * }
 * // Status was OK, so create a Client with the given Credentials.
 * gcs::Client client(gcs::ClientOptions(*creds));
 *
 * // Use service account credentials from a JSON keyfile:
 * std::string path = "/path/to/keyfile.json";
 * auto creds =
 *     gcs::oauth2::CreateServiceAccountCredentialsFromJsonFilePath(path);
 * if (!creds) {
 *   // Handle failure and return.
 * }
 * gcs::Client client(gcs::ClientOptions(*creds));
 *
 * // Use Compute Engine credentials for the instance's default service account.
 * gcs::Client client(
 *     gcs::ClientOptions(gcs::oauth2::CreateComputeEngineCredentials()));
 *
 * // Use no credentials:
 * gcs::Client client(
 *     gcs::ClientOptions(gcs::oauth2::CreateAnonymousCredentials()));
 * @endcode
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v1::StatusOr) for more details.
 *
 * @code
 * namespace gcs = google::cloud::storage;
 * gcs::Client client = ...;
 * google::cloud::StatusOr<gcs::BucketMetadata> bucket_metadata =
 *     client.GetBucketMetadata("my-bucket");
 *
 * if (!bucket_metadata) {
 *   std::cerr << "Error getting metadata for my-bucket: "
 *             << bucket_metadata.status() << "\n";
 *   return;
 * }
 *
 * // Use bucket_metadata as a smart pointer here, e.g.:
 * std::cout << "The generation for " << bucket_metadata->name() " is "
 *           << bucket_metadata->generation() << "\n";
 * @endcode
 *
 * In addition, the @ref index "main page" contains examples using `StatusOr<T>`
 * to handle errors.
 *
 * @par Retry, Backoff, and Idempotency Policies
 *
 * The library automatically retries requests that fail with transient errors,
 * and follows the
 * [recommended
 * practice](https://cloud.google.com/storage/docs/exponential-backoff) to
 * backoff between retries.
 *
 * The default policies are to continue retrying for up to 15 minutes, and to
 * use truncated (at 5 minutes) exponential backoff, doubling the maximum
 * backoff period between retries. Likewise, the idempotency policy is
 * configured to retry all operations.
 *
 * The application can override these policies when constructing objects of this
 * class. The documentation for the constructors show examples of this in
 * action.
 *
 * @see https://cloud.google.com/storage/ for an overview of GCS.
 *
 * @see https://cloud.google.com/storage/docs/key-terms for an introduction of
 *     the key terms used in GCS.
 *
 * @see https://cloud.google.com/storage/docs/json_api/ for an overview of the
 *     underlying API.
 *
 * @see https://cloud.google.com/docs/authentication/production for details
 *     about Application Default %Credentials.
 *
 * @see #google::cloud::v1::StatusOr.
 *
 * @see `LimitedTimeRetryPolicy` and `LimitedErrorCountRetryPolicy` for
 * alternative retry policies.
 *
 * @see `ExponentialBackoffPolicy` to configure different parameters for the
 * exponential backoff policy.
 *
 * @see `AlwaysRetryIdempotencyPolicy` and `StrictIdempotencyPolicy` for
 * alternative idempotency policies.
 */
class Client {
 public:
  /**
   * Creates the default client type given the options.
   *
   * @param options the client options, these are used to control credentials,
   *   buffer sizes, etc.
   * @param policies the client policies, these control the behavior of the
   *   client, for example, how to backoff when an operation needs to be
   *   retried, or what operations cannot be retried because they are not
   *   idempotent.
   *
   * @par Idempotency Policy Example
   * @snippet storage_object_samples.cc insert object strict idempotency
   *
   * @par Modified Retry Policy Example
   * @snippet storage_object_samples.cc insert object modified retry
   */
  template <typename... Policies>
  explicit Client(ClientOptions options, Policies&&... policies)
      : Client(CreateDefaultInternalClient(std::move(options)),
               std::forward<Policies>(policies)...) {}

  /**
   * Creates the default client type given the credentials and policies.
   *
   * @param credentials a set of credentials to initialize the `ClientOptions`.
   * @param policies the client policies, these control the behavior of the
   *   client, for example, how to backoff when an operation needs to be
   *   retried, or what operations cannot be retried because they are not
   *   idempotent.
   *
   * @par Idempotency Policy Example
   * @snippet storage_object_samples.cc insert object strict idempotency
   *
   * @par Modified Retry Policy Example
   * @snippet storage_object_samples.cc insert object modified retry
   */
  template <typename... Policies>
  explicit Client(std::shared_ptr<oauth2::Credentials> credentials,
                  Policies&&... policies)
      : Client(ClientOptions(std::move(credentials)),
               std::forward<Policies>(policies)...) {}

  /// Builds a client and maybe override the retry, idempotency, and/or backoff
  /// policies.
  template <typename... Policies>
  explicit Client(std::shared_ptr<internal::RawClient> client,
                  Policies&&... policies)
      : raw_client_(
            Decorate(std::move(client), std::forward<Policies>(policies)...)) {}

  /// Define a tag to disable automatic decorations of the RawClient.
  struct NoDecorations {};

  /// Builds a client with a specific RawClient, without decorations.
  explicit Client(std::shared_ptr<internal::RawClient> client, NoDecorations)
      : raw_client_(std::move(client)) {}

  /// Create a Client using ClientOptions::CreateDefaultClientOptions().
  static StatusOr<Client> CreateDefaultClient();

  /// Access the underlying `RawClient`.
  std::shared_ptr<internal::RawClient> raw_client() const {
    return raw_client_;
  }

  //@{
  /**
   * @name Bucket operations.
   *
   * Buckets are the basic containers that hold your data. Everything that you
   * store in GCS must be contained in a bucket. You can use buckets to organize
   * your data and control access to your data, but unlike directories and
   * folders, you cannot nest buckets.
   *
   * @see https://cloud.google.com/storage/docs/key-terms#buckets for more
   * information about GCS buckets.
   */
  /**
   * Fetches the list of buckets for a given project.
   *
   * @param project_id the project to query.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `MaxResults`, `Prefix`,
   *     `UserProject`, and `Projection`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc list buckets for project
   */
  template <typename... Options>
  ListBucketsReader ListBucketsForProject(std::string const& project_id,
                                          Options&&... options) {
    internal::ListBucketsRequest request(project_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto client = raw_client_;
    return google::cloud::internal::MakePaginationRange<ListBucketsReader>(
        request,
        [client](internal::ListBucketsRequest const& r) {
          return client->ListBuckets(r);
        },
        [](internal::ListBucketsResponse r) { return std::move(r.items); });
  }

  /**
   * Fetches the list of buckets for the default project.
   *
   * The default project is required to be configured in the `ClientOptions`
   * used to construct this object. If the application does not set the project
   * id in the `ClientOptions`, the value of the `GOOGLE_CLOUD_PROJECT` is
   * used. If neither the environment variable is set, nor a value is set
   * explicitly by the application, the returned `ListBucketsReader` will
   * return an error status when used.
   *
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `MaxResults`, `Prefix`,
   *     `UserProject`, and `Projection`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc list buckets
   */
  template <typename... Options>
  ListBucketsReader ListBuckets(Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    return ListBucketsForProject(project_id, std::forward<Options>(options)...);
  }

  /**
   * Creates a new Google Cloud Storage bucket using the default project. If
   * the default project is not configured the server will reject the request,
   * and this function returns the error status.
   *
   * @param bucket_name the name of the new bucket.
   * @param metadata the metadata for the new Bucket.  The `name` field is
   *     ignored in favor of @p bucket_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `PredefinedAcl`,
   *     `PredefinedDefaultObjectAcl`, `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is always idempotent. It fails if the bucket already exists.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc create bucket
   *
   * @see Before enabling Uniform Bucket Level Access please review the
   *     [feature documentation][ubla-link], as well as
   *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
   *
   * [ubla-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access
   * [ubla-should-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
   */
  template <typename... Options>
  StatusOr<BucketMetadata> CreateBucket(std::string bucket_name,
                                        BucketMetadata metadata,
                                        Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    return CreateBucketForProject(std::move(bucket_name), project_id,
                                  std::move(metadata),
                                  std::forward<Options>(options)...);
  }

  /**
   * Creates a new Google Cloud Storage Bucket in a given project.
   *
   * @param bucket_name the name of the new bucket.
   * @param project_id the id of the project that will host the new bucket.
   * @param metadata the metadata for the new Bucket.  The `name` field is
   *     ignored in favor of @p bucket_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `PredefinedAcl`,
   *     `PredefinedDefaultObjectAcl`, `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is always idempotent. It fails if the bucket already exists.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc create bucket for project
   *
   * @see Before enabling Uniform Bucket Level Access please review the
   *     [feature documentation][ubla-link], as well as
   *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
   *
   * [ubla-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access
   * [ubla-should-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
   */
  template <typename... Options>
  StatusOr<BucketMetadata> CreateBucketForProject(std::string bucket_name,
                                                  std::string project_id,
                                                  BucketMetadata metadata,
                                                  Options&&... options) {
    metadata.set_name(std::move(bucket_name));
    internal::CreateBucketRequest request(std::move(project_id),
                                          std::move(metadata));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateBucket(request);
  }

  /**
   * Fetches the bucket metadata.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `UserProject`, and `Projection`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc get bucket metadata
   */
  template <typename... Options>
  StatusOr<BucketMetadata> GetBucketMetadata(std::string const& bucket_name,
                                             Options&&... options) {
    internal::GetBucketMetadataRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetBucketMetadata(request);
  }

  /**
   * Deletes a Google Cloud Storage Bucket.
   *
   * @param bucket_name the bucket to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc delete bucket
   */
  template <typename... Options>
  Status DeleteBucket(std::string const& bucket_name, Options&&... options) {
    internal::DeleteBucketRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteBucket(request).status();
  }

  /**
   * Updates the metadata in a Google Cloud Storage Bucket.
   *
   * A `Buckets: update` request changes *all* the writeable attributes of a
   * bucket, in contrast, a `Buckets: patch` request only changes the subset of
   * the attributes included in the request. This function creates a
   * `Buckets: update` request to change the writable attributes in
   * `BucketMetadata`.
   *
   * @param bucket_name the name of the new bucket.
   * @param metadata the new metadata for the Bucket.  The `name` field is
   *     ignored in favor of @p bucket_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `PredefinedAcl`,
   *     `PredefinedDefaultObjectAcl`, `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case,`IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc update bucket
   *
   * @see Before enabling Uniform Bucket Level Access please review the
   *     [feature documentation][ubla-link], as well as
   *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
   *
   * [ubla-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access
   * [ubla-should-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
   */
  template <typename... Options>
  StatusOr<BucketMetadata> UpdateBucket(std::string bucket_name,
                                        BucketMetadata metadata,
                                        Options&&... options) {
    metadata.set_name(std::move(bucket_name));
    internal::UpdateBucketRequest request(std::move(metadata));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateBucket(request);
  }

  /**
   * Computes the difference between two BucketMetadata objects and patches a
   * bucket based on that difference.
   *
   * A `Buckets: update` request changes *all* the writeable attributes of a
   * bucket, in contrast, a `Buckets: patch` request only changes the subset of
   * the attributes included in the request.
   *
   * This function creates a patch request to change the writeable attributes in
   * @p original to the values in @p updated.  Non-writeable attributes are
   * ignored, and attributes not present in @p updated are removed. Typically
   * this function is used after the application obtained a value with
   * `GetBucketMetadata` and has modified these parameters.
   *
   * @param bucket_name the bucket to be updated.
   * @param original the initial value of the bucket metadata.
   * @param updated the updated value for the bucket metadata.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc patch bucket storage class
   *
   * @see Before enabling Uniform Bucket Level Access please review the
   *     [feature documentation][ubla-link], as well as
   *     ["Should you use uniform bucket-level access?"][ubla-should-link].
   *
   * [ubla-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access
   * [ubla-should-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
   */
  template <typename... Options>
  StatusOr<BucketMetadata> PatchBucket(std::string bucket_name,
                                       BucketMetadata const& original,
                                       BucketMetadata const& updated,
                                       Options&&... options) {
    internal::PatchBucketRequest request(std::move(bucket_name), original,
                                         updated);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucket(request);
  }

  /**
   * Patches the metadata in a Google Cloud Storage Bucket given a desired set
   * changes.
   *
   * A `Buckets: update` request changes *all* the writeable attributes of a
   * bucket, in contrast, a `Buckets: patch` request only changes the subset of
   * the attributes included in the request. This function creates a patch
   * request based on the given `BucketMetadataPatchBuilder` which represents
   * the desired set of changes.
   *
   * @param bucket_name the bucket to be updated.
   * @param builder the set of updates to perform in the Bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_bucket_samples.cc patch bucket storage class with builder
   *
   * @see Before enabling Uniform Bucket Level Access please review the
   *     [feature documentation][ubla-link], as well as
   *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
   *
   * [ubla-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access
   * [ubla-should-link]:
   * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
   */
  template <typename... Options>
  StatusOr<BucketMetadata> PatchBucket(
      std::string bucket_name, BucketMetadataPatchBuilder const& builder,
      Options&&... options) {
    internal::PatchBucketRequest request(std::move(bucket_name), builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucket(request);
  }

  /**
   * Fetches the [IAM policy](@ref google::cloud::v1::IamPolicy) for a Bucket.
   *
   * Google Cloud Identity & Access Management (IAM) lets administrators
   * authorize who can take action on specific resources, including Google
   * Cloud Storage Buckets. This operation allows you to query the IAM policies
   * for a Bucket. IAM policies are a superset of the Bucket ACL, changes
   * to the Bucket ACL are reflected in the IAM policy, and vice-versa. The
   * documentation describes
   * [the
   * mapping](https://cloud.google.com/storage/docs/access-control/iam#acls)
   * between legacy Bucket ACLs and IAM policies.
   *
   * Consult
   * [the
   * documentation](https://cloud.google.com/storage/docs/access-control/iam)
   * for a more detailed description of IAM policies and their use in
   * Google Cloud Storage.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @deprecated this function is deprecated; it doesn't support conditional
   *     bindings and will not support any other features to come; please use
   *     `GetNativeBucketIamPolicy` instead.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * Use #GetNativeBucketIamPolicy() instead.
   *
   * @see #google::cloud::v1::IamPolicy for details about the `IamPolicy` class.
   */
  template <typename... Options>
  GOOGLE_CLOUD_CPP_STORAGE_IAM_DEPRECATED("GetNativeBucketIamPolicy")
  StatusOr<IamPolicy> GetBucketIamPolicy(std::string const& bucket_name,
                                         Options&&... options) {
    internal::GetBucketIamPolicyRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetBucketIamPolicy(request);
  }

  /**
   * Fetches the native [IAM policy](@ref google::cloud::v1::IamPolicy) for a
   * Bucket.
   *
   * Google Cloud Identity & Access Management (IAM) lets administrators
   * authorize who can take action on specific resources, including Google
   * Cloud Storage Buckets. This operation allows you to query the IAM policies
   * for a Bucket. IAM policies are a superset of the Bucket ACL, changes
   * to the Bucket ACL are reflected in the IAM policy, and vice-versa. The
   * documentation describes
   * [the
   * mapping](https://cloud.google.com/storage/docs/access-control/iam#acls)
   * between legacy Bucket ACLs and IAM policies.
   *
   * Consult
   * [the
   * documentation](https://cloud.google.com/storage/docs/access-control/iam)
   * for a more detailed description of IAM policies and their use in
   * Google Cloud Storage.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_iam_samples.cc native get bucket iam policy
   *
   * @see #google::cloud::v1::IamPolicy for details about the `IamPolicy` class.
   */
  template <typename... Options>
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      std::string const& bucket_name, Options&&... options) {
    internal::GetBucketIamPolicyRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetNativeBucketIamPolicy(request);
  }

  /**
   * Sets the [IAM Policy](@ref google::cloud::v1::IamPolicy) for a Bucket.
   *
   * Google Cloud Identity & Access Management (IAM) lets administrators
   * authorize who can take action on specific resources, including Google
   * Cloud Storage Buckets. This operation allows you to set the IAM policies
   * for a Bucket. IAM policies are a superset of the Bucket ACL, changes
   * to the Bucket ACL are reflected in the IAM policy, and vice-versa. The
   * documentation describes
   * [the
   * mapping](https://cloud.google.com/storage/docs/access-control/iam#acls)
   * between legacy Bucket ACLs and IAM policies.
   *
   * Consult
   * [the
   * documentation](https://cloud.google.com/storage/docs/access-control/iam)
   * for a more detailed description of IAM policies their use in
   * Google Cloud Storage.
   *
   * @note The server rejects requests where the ETag value of the policy does
   *   not match the current ETag. Effectively this means that applications must
   *   use `GetBucketIamPolicy()` to fetch the current value and ETag before
   *   calling `SetBucketIamPolicy()`. Applications should use optimistic
   *   concurrency control techniques to retry changes in case some other
   *   application modified the IAM policy between the `GetBucketIamPolicy`
   *   and `SetBucketIamPolicy` calls.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param iam_policy the new IAM policy.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @deprecated this function is deprecated; it doesn't support conditional
   *     bindings and will not support any other features to come; please use
   *     `SetNativeBucketIamPolicy` instead.
   *
   * @par Example: adding a new member
   * Use #GetNativeBucketIamPolicy() instead.
   *
   * @see #google::cloud::v1::IamPolicy for details about the `IamPolicy` class.
   */
  template <typename... Options>
  GOOGLE_CLOUD_CPP_STORAGE_IAM_DEPRECATED("SetNativeBucketIamPolicy")
  StatusOr<IamPolicy> SetBucketIamPolicy(std::string const& bucket_name,
                                         IamPolicy const& iam_policy,
                                         Options&&... options) {
    internal::SetBucketIamPolicyRequest request(bucket_name, iam_policy);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->SetBucketIamPolicy(request);
  }

  /**
   * Sets the native [IAM Policy](@ref google::cloud::v1::IamPolicy) for a
   * Bucket.
   *
   * Google Cloud Identity & Access Management (IAM) lets administrators
   * authorize who can take action on specific resources, including Google
   * Cloud Storage Buckets. This operation allows you to set the IAM policies
   * for a Bucket. IAM policies are a superset of the Bucket ACL, changes
   * to the Bucket ACL are reflected in the IAM policy, and vice-versa. The
   * documentation describes
   * [the
   * mapping](https://cloud.google.com/storage/docs/access-control/iam#acls)
   * between legacy Bucket ACLs and IAM policies.
   *
   * Consult
   * [the
   * documentation](https://cloud.google.com/storage/docs/access-control/iam)
   * for a more detailed description of IAM policies their use in
   * Google Cloud Storage.
   *
   * @note The server rejects requests where the ETag value of the policy does
   *   not match the current ETag. Effectively this means that applications must
   *   use `GetBucketIamPolicy()` to fetch the current value and ETag before
   *   calling `SetBucketIamPolicy()`. Applications should use optimistic
   *   concurrency control techniques to retry changes in case some other
   *   application modified the IAM policy between the `GetBucketIamPolicy`
   *   and `SetBucketIamPolicy` calls.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param iam_policy the new IAM policy.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example: adding a new member
   * @snippet storage_bucket_iam_samples.cc native add bucket iam member
   *
   * @par Example: removing a IAM member
   * @snippet storage_bucket_iam_samples.cc native remove bucket iam member
   *
   * @see #google::cloud::v1::IamPolicy for details about the `IamPolicy` class.
   */
  template <typename... Options>
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      std::string const& bucket_name, NativeIamPolicy const& iam_policy,
      Options&&... options) {
    internal::SetNativeBucketIamPolicyRequest request(bucket_name, iam_policy);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->SetNativeBucketIamPolicy(request);
  }

  /**
   * Tests the IAM permissions of the caller against a Bucket.
   *
   * Google Cloud Identity & Access Management (IAM) lets administrators
   * authorize who can take action on specific resources, including Google
   * Cloud Storage Buckets. This operation tests the permissions of the caller
   * for a Bucket. You must provide a list of permissions, this API will return
   * the subset of those permissions that the current caller has in the given
   * Bucket.
   *
   * Consult
   * [the
   * documentation](https://cloud.google.com/storage/docs/access-control/iam)
   * for a more detailed description of IAM policies their use in
   * Google Cloud Storage.
   *
   * @param bucket_name query metadata information about this bucket.
   * @param permissions the list of permissions to check.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_iam_samples.cc test bucket iam permissions
   */
  template <typename... Options>
  StatusOr<std::vector<std::string>> TestBucketIamPermissions(
      std::string bucket_name, std::vector<std::string> permissions,
      Options&&... options) {
    internal::TestBucketIamPermissionsRequest request(std::move(bucket_name),
                                                      std::move(permissions));
    request.set_multiple_options(std::forward<Options>(options)...);
    auto result = raw_client_->TestBucketIamPermissions(request);
    if (!result) {
      return std::move(result).status();
    }
    return std::move(result.value().permissions);
  }

  /**
   * Locks the retention policy for a bucket.
   *
   * @warning Locking a retention policy is an irreversible action. Once locked,
   *     you must delete the entire bucket in order to "remove" the bucket's
   *     retention policy. However, before you can delete the bucket, you must
   *     be able to delete all the objects in the bucket, which itself is only
   *     possible if all the objects have reached the retention period set by
   *     the retention policy.
   *
   * The [Bucket Lock
   * feature](https://cloud.google.com/storage/docs/bucket-lock) allows you to
   * configure a data retention policy for a Cloud Storage bucket that governs
   * how long objects in the bucket must be retained. The feature also allows
   * you to lock the data retention policy, permanently preventing the policy
   * from being reduced or removed.
   *
   * @param bucket_name the name of the bucket.
   * @param metageneration the expected value of the metageneration on the
   *     bucket. The request will fail if the metageneration does not match the
   *     current value.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is always idempotent because the `metageneration` parameter
   * is always required, and it acts as a pre-condition on the operation.
   *
   * @par Example: lock the retention policy
   * @snippet storage_retention_policy_samples.cc lock retention policy
   *
   * @par Example: get the current retention policy
   * @snippet storage_retention_policy_samples.cc get retention policy
   *
   * @par Example: set the current retention policy
   * @snippet storage_retention_policy_samples.cc set retention policy
   *
   * @par Example: remove the retention policy
   * @snippet storage_retention_policy_samples.cc remove retention policy
   *
   * @see https://cloud.google.com/storage/docs/bucket-lock for a description of
   *     the Bucket Lock feature.
   *
   * @see https://cloud.google.com/storage/docs/using-bucket-lock for examples
   *     of how to use the Bucket Lock and retention policy features.
   */
  template <typename... Options>
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      std::string const& bucket_name, std::uint64_t metageneration,
      Options&&... options) {
    internal::LockBucketRetentionPolicyRequest request(bucket_name,
                                                       metageneration);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->LockBucketRetentionPolicy(request);
  }
  //@}

  //@{
  /**
   * @name Object operations
   *
   * Objects are the individual pieces of data that you store in GCS. Objects
   * have two components: *object data* and *object metadata*. Object data
   * (sometimes referred to as *media*) is typically a file that you want
   * to store in GCS. Object metadata is information that describe various
   * object qualities.
   *
   * @see https://cloud.google.com/storage/docs/key-terms#objects for more
   * information about GCS objects.
   */
  /**
   * Creates an object given its name and contents.
   *
   * @param bucket_name the name of the bucket that will contain the object.
   * @param object_name the name of the object to be created.
   * @param contents the contents (media) for the new object.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `ContentEncoding`,
   *     `ContentType`, `Crc32cChecksumValue`, `DisableCrc32cChecksum`,
   *     `DisableMD5Hash`, `EncryptionKey`, `IfGenerationMatch`,
   *     `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `KmsKeyName`, `MD5HashValue`,
   *     `PredefinedAcl`, `Projection`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc insert object
   *
   * @par Example
   * @snippet storage_object_samples.cc insert object multipart
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> InsertObject(std::string const& bucket_name,
                                        std::string const& object_name,
                                        std::string contents,
                                        Options&&... options) {
    internal::InsertObjectMediaRequest request(bucket_name, object_name,
                                               std::move(contents));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->InsertObjectMedia(request);
  }

  /**
   * Copies an existing object.
   *
   * Use `CopyObject` to copy between objects in the same location and storage
   * class.  Copying objects across locations or storage classes can fail for
   * large objects and retrying the operation will not succeed.
   *
   * @note Prefer using `RewriteObject()` to copy objects, `RewriteObject()` can
   *     copy objects to different locations, with different storage class,
   *     and/or with different encryption keys.
   *
   * @param source_bucket_name the name of the bucket that contains the object
   *     to be copied.
   * @param source_object_name the name of the object to copy.
   * @param destination_bucket_name the name of the bucket that will contain the
   *     new object.
   * @param destination_object_name the name of the new object.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationPredefinedAcl`,
   *     `EncryptionKey`, `IfGenerationMatch`, `IfGenerationNotMatch`,
   *     `IfMetagenerationMatch`, `IfMetagenerationNotMatch`,
   *     `IfSourceGenerationMatch`, `IfSourceGenerationNotMatch`,
   *     `IfSourceMetagenerationMatch`, `IfSourceMetagenerationNotMatch`,
   *     `Projection`, `SourceGeneration`, `UserProject`, and
   *     `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc copy object
   *
   * @par Example: copy an encrypted object
   * @snippet storage_object_csek_samples.cc copy encrypted object
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/objects/copy for
   *   a full description of the advantages of `Objects: rewrite` over
   *   `Objects: copy`.
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> CopyObject(std::string source_bucket_name,
                                      std::string source_object_name,
                                      std::string destination_bucket_name,
                                      std::string destination_object_name,
                                      Options&&... options) {
    internal::CopyObjectRequest request(
        std::move(source_bucket_name), std::move(source_object_name),
        std::move(destination_bucket_name), std::move(destination_object_name));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CopyObject(request);
  }

  /**
   * Fetches the object metadata.
   *
   * @param bucket_name the bucket containing the object.
   * @param object_name the object name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_samples.cc get object metadata
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> GetObjectMetadata(std::string const& bucket_name,
                                             std::string const& object_name,
                                             Options&&... options) {
    internal::GetObjectMetadataRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetObjectMetadata(request);
  }

  /**
   * Lists the objects in a bucket.
   *
   * @param bucket_name the name of the bucket to list.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include
   *     `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `UserProject`,
   *     `Projection`, `Prefix`, `Delimiter`, `IncludeTrailingDelimiter`,
   *     `StartOffset`, `EndOffset`, and `Versions`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_samples.cc list objects
   */
  template <typename... Options>
  ListObjectsReader ListObjects(std::string const& bucket_name,
                                Options&&... options) {
    internal::ListObjectsRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto client = raw_client_;
    return google::cloud::internal::MakePaginationRange<ListObjectsReader>(
        request,
        [client](internal::ListObjectsRequest const& r) {
          return client->ListObjects(r);
        },
        [](internal::ListObjectsResponse r) { return std::move(r.items); });
  }

  /**
   * Lists the objects and prefixes in a bucket.
   *
   * @param bucket_name the name of the bucket to list.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include
   *     `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `UserProject`,
   *     `Projection`, `Prefix`, `Delimiter`, `IncludeTrailingDelimiter`,
   *     `StartOffset`, `EndOffset`, and `Versions`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_samples.cc list objects and prefixes
   */
  template <typename... Options>
  ListObjectsAndPrefixesReader ListObjectsAndPrefixes(
      std::string const& bucket_name, Options&&... options) {
    internal::ListObjectsRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto client = raw_client_;
    return google::cloud::internal::MakePaginationRange<
        ListObjectsAndPrefixesReader>(
        request,
        [client](internal::ListObjectsRequest const& r) {
          return client->ListObjects(r);
        },
        [](internal::ListObjectsResponse r) {
          std::vector<ObjectOrPrefix> result;
          for (auto& item : r.items) {
            result.emplace_back(std::move(item));
          }
          for (auto& prefix : r.prefixes) {
            result.emplace_back(std::move(prefix));
          }
          internal::SortObjectsAndPrefixes(result);
          return result;
        });
  }

  /**
   * Reads the contents of an object.
   *
   * Returns an object derived from `std::istream` which can be used to read the
   * contents of the GCS blob. The application should check the `badbit` (e.g.
   * by calling `stream.bad()`) on the returned object to detect if there was
   * an error reading from the blob. If `badbit` is set, the application can
   * check the `status()` variable to get details about the failure.
   * Applications can also set the exception mask on the returned stream, in
   * which case an exception is thrown if an error is detected.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DisableCrc32cChecksum`,
   *     `DisableMD5Hash`, `IfGenerationMatch`, `EncryptionKey`, `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `ReadFromOffset`, `ReadRange`, `ReadLast`
   *     and `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_samples.cc read object
   *
   * @par Example
   * @snippet storage_object_samples.cc read object range
   *
   * @par Example: read a object encrypted with a CSEK.
   * @snippet storage_object_csek_samples.cc read encrypted object
   */
  template <typename... Options>
  ObjectReadStream ReadObject(std::string const& bucket_name,
                              std::string const& object_name,
                              Options&&... options) {
    struct HasReadRange
        : public absl::disjunction<std::is_same<ReadRange, Options>...> {};
    struct HasReadFromOffset
        : public absl::disjunction<std::is_same<ReadFromOffset, Options>...> {};
    struct HasReadLast
        : public absl::disjunction<std::is_same<ReadLast, Options>...> {};

    struct HasIncompatibleRangeOptions
        : public std::integral_constant<bool, HasReadLast::value &&
                                                  (HasReadFromOffset::value ||
                                                   HasReadRange::value)> {};

    static_assert(!HasIncompatibleRangeOptions::value,
                  "Cannot set ReadLast option with either ReadFromOffset or "
                  "ReadRange.");

    internal::ReadObjectRangeRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return ReadObjectImpl(request);
  }

  /**
   * Writes contents into an object.
   *
   * This creates a `std::ostream` object to upload contents. The application
   * can use either the regular `operator<<()`, or `std::ostream::write()` to
   * upload data.
   *
   * This function always uses [resumable uploads][resumable-link]. The
   * application can provide a `#RestoreResumableUploadSession()` option to
   * resume a previously created upload. The returned object has accessors to
   * query the session id and the next byte expected by GCS.
   *
   * @note When resuming uploads it is the application's responsibility to save
   *     the session id to restart the upload later. Likewise, it is the
   *     application's responsibility to query the next expected byte and send
   *     the remaining data without gaps or duplications.
   *
   * For small uploads we recommend using `InsertObject`, consult
   * [the documentation][how-to-upload-link] for details.
   *
   * If the application does not provide a `#RestoreResumableUploadSession()`
   * option, or it provides the `#NewResumableUploadSession()` option then a new
   * resumable upload session is created.
   *
   * To perform efficient uploads applications should consider using unbuffered
   * I/O operations on the returned stream (aka `std::ostream::write()`).
   * Application developers should consider the following properties of
   * resumable uploads to use this API efficiently:
   *
   * - Resumable uploads are performed in "chunks" sent to GCS, these chunks
   *   are committed and saved at least until the session is deleted (or garbage
   *   collected, approximately after 7 days.)
   * - The size of these chunks (except the last one) must be a multiple of the
   *   upload quantum (256KiB).
   * - Uploading a chunk that is not a multiple of the quantum implicitly
   *   finalizes the upload.
   *
   * The library, therefore, *must* buffer any data that does not fill a full
   * quantum. If you are interested in avoiding data copies you should always
   * provide the library with buffers that fit in this quantum.
   *
   * In addition, applications should consider providing large buffers as the
   * library waits until GCS confirms that the chunk is uploaded before
   * returning control to the application. Naturally there is a tradeoff between
   * copying data to prepare large buffers vs. sending many small buffers and
   * paying the network costs for each.  We recommend that you use buffers in
   * the 16MiB to 64MiB range for best performance.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param options a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `ContentEncoding`, `ContentType`,
   *   `Crc32cChecksumValue`, `DisableCrc32cChecksum`, `DisableMD5Hash`,
   *   `EncryptionKey`, `IfGenerationMatch`, `IfGenerationNotMatch`,
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `KmsKeyName`,
   *   `MD5HashValue`, `PredefinedAcl`, `Projection`,
   *   `UseResumableUploadSession`, `UserProject`, `WithObjectMetadata` and
   *   `UploadContentLength`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc write object
   *
   * @par Example: write an object with a CMEK.
   * @snippet storage_object_cmek_samples.cc write object with kms key
   *
   * @par Example: starting a resumable upload.
   * @snippet storage_object_resumable_write_samples.cc start resumable upload
   *
   * @par Example: resuming a resumable upload.
   * @snippet storage_object_resumable_write_samples.cc resume resumable upload
   *
   * @see [Resumable Uploads][resumable-link] for more information about
   *     resumable uploads.
   *
   * [resumable-link]: https://cloud.google.com/storage/docs/resumable-uploads
   * [how-to-upload-link]:
   * https://cloud.google.com/storage/docs/json_api/v1/how-tos/upload
   */
  template <typename... Options>
  ObjectWriteStream WriteObject(std::string const& bucket_name,
                                std::string const& object_name,
                                Options&&... options) {
    internal::ResumableUploadRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return WriteObjectImpl(request);
  }

  /**
   * Uploads a file to an object.
   *
   * @note
   * Only regular files are supported. If you need to upload the results of
   * reading a device, Named Pipe, FIFO, or other type of file system object
   * that is **not** a regular file then `WriteObject()` is probably a better
   * alternative.
   *
   * @param file_name the name of the file to be uploaded.
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be read.
   * @param options a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `ContentEncoding`, `ContentType`,
   *   `Crc32cChecksumValue`, `DisableCrc32cChecksum`, `DisableMD5Hash`,
   *   `EncryptionKey`, `IfGenerationMatch`, `IfGenerationNotMatch`,
   *   `IfMetagenerationMatch`, `IfMetagenerationNotMatch`, `KmsKeyName`,
   *   `MD5HashValue`, `PredefinedAcl`, `Projection`, `UserProject`,
   *   `UploadFromOffset`, `UploadLimit` and `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_file_transfer_samples.cc upload file
   *
   * @par Example: manually selecting a resumable upload
   * @snippet storage_object_file_transfer_samples.cc upload file resumable
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> UploadFile(std::string const& file_name,
                                      std::string const& bucket_name,
                                      std::string const& object_name,
                                      Options&&... options) {
    // Determine, at compile time, which version of UploadFileImpl we should
    // call. This needs to be done at compile time because ObjectInsertMedia
    // does not support (nor should it support) the UseResumableUploadSession
    // option.
    using HasUseResumableUpload =
        absl::disjunction<std::is_same<UseResumableUploadSession, Options>...>;
    return UploadFileImpl(file_name, bucket_name, object_name,
                          HasUseResumableUpload{},
                          std::forward<Options>(options)...);
  }

  /**
   * Cancel a resumable upload.
   *
   * @param upload_session_url the url of the upload session. Returned by
   * `ObjectWriteStream::resumable_session_id`.
   * @param options a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is always idempotent because it only acts on a specific
   * `upload_session_url`.
   */
  template <typename... Options>
  Status DeleteResumableUpload(std::string const& upload_session_url,
                               Options&&... options) {
    internal::DeleteResumableUploadRequest request(upload_session_url);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteResumableUpload(request).status();
  }

  /**
   * Downloads a Cloud Storage object to a file.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be downloaded.
   * @param file_name the name of the destination file that will have the object
   *   media.
   * @param options a list of optional query parameters and/or request headers.
   *   Valid types for this operation include `IfGenerationMatch`,
   *   `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *   `IfMetagenerationNotMatch`, `Generation`, `ReadFromOffset`, `ReadRange`,
   *   and `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_file_transfer_samples.cc download file
   */
  template <typename... Options>
  Status DownloadToFile(std::string const& bucket_name,
                        std::string const& object_name,
                        std::string const& file_name, Options&&... options) {
    internal::ReadObjectRangeRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return DownloadFileImpl(request, file_name);
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
   *
   * @par Idempotency
   * This operation is only idempotent if:
   * - restricted by pre-conditions, in this case, `IfGenerationMatch`
   * - or, if it applies to only one object version via `Generation`.
   *
   * @par Example
   * @snippet storage_object_samples.cc delete object
   */
  template <typename... Options>
  Status DeleteObject(std::string const& bucket_name,
                      std::string const& object_name, Options&&... options) {
    internal::DeleteObjectRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteObject(request).status();
  }

  /**
   * Updates the metadata in a Google Cloud Storage Object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param metadata the new metadata for the Object.  Only the writeable fields
   *     accepted by the `Objects: update` API are used, all other fields are
   *     ignored. In particular, note that `bucket` and `name` are ignored in
   *     favor of @p bucket_name and @p object_name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `PredefinedAcl`,
   *     `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc update object metadata
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> UpdateObject(std::string bucket_name,
                                        std::string object_name,
                                        ObjectMetadata metadata,
                                        Options&&... options) {
    internal::UpdateObjectRequest request(
        std::move(bucket_name), std::move(object_name), std::move(metadata));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateObject(request);
  }

  /**
   * Patches the metadata in a Google Cloud Storage Object.
   *
   * This function creates a patch request to change the writeable attributes in
   * @p original to the values in @p updated.  Non-writeable attributes are
   * ignored, and attributes not present in @p updated are removed. Typically
   * this function is used after the application obtained a value with
   * `GetObjectMetadata` and has modified these parameters.
   *
   * @param bucket_name the bucket that contains the object to be updated.
   * @param object_name the object to be updated.
   * @param original the initial value of the object metadata.
   * @param updated the updated value for the object metadata.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `PredefinedAcl`,
   *     `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc patch object delete metadata
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> PatchObject(std::string bucket_name,
                                       std::string object_name,
                                       ObjectMetadata const& original,
                                       ObjectMetadata const& updated,
                                       Options&&... options) {
    internal::PatchObjectRequest request(
        std::move(bucket_name), std::move(object_name), original, updated);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchObject(request);
  }

  /**
   * Patches the metadata in a Google Cloud Storage Object.
   *
   * This function creates a patch request based on the given @p builder.
   * Typically this function is used when the application needs to set an
   * object's metadata fields regardless of their previous value (i.e. when
   * calling GetObjectMetadata first is not necessary).
   *
   * @param bucket_name the bucket that contains the object to be updated.
   * @param object_name the object to be updated.
   * @param builder the set of updates to perform in the Object metadata.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`,
   *     `IfGenerationMatch`, `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch`, `PredefinedAcl`,
   *     `Projection`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfMetagenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc patch object content type
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> PatchObject(
      std::string bucket_name, std::string object_name,
      ObjectMetadataPatchBuilder const& builder, Options&&... options) {
    internal::PatchObjectRequest request(std::move(bucket_name),
                                         std::move(object_name), builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchObject(request);
  }

  /**
   * Composes existing objects into a new object in the same bucket.
   *
   * @param bucket_name the name of the bucket used for source object and
   *     destination object.
   * @param source_objects objects used to compose `destination_object_name`.
   * @param destination_object_name the composed object name.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include
   *      `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *      `IfMetagenerationMatch`, `KmsKeyName`, `UserProject`, and
   *      `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_samples.cc compose object
   *
   * @par Example: using encrypted objects with CSEK
   * @snippet storage_object_csek_samples.cc compose object csek
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> ComposeObject(
      std::string bucket_name, std::vector<ComposeSourceObject> source_objects,
      std::string destination_object_name, Options&&... options) {
    internal::ComposeObjectRequest request(std::move(bucket_name),
                                           std::move(source_objects),
                                           std::move(destination_object_name));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->ComposeObject(request);
  }

  /**
   * Creates an `ObjectRewriter` to copy the source object.
   *
   * Applications use this function to reliably copy objects across [location
   * boundaries](https://cloud.google.com/storage/docs/locations), and to
   * rewrite objects with different encryption keys. The operation returns a
   * `ObjectRewriter`, which the application can use to initiate the copy and to
   * iterate if the copy requires more than one call to complete.
   *
   * @note Application developers should be aware that rewriting large objects
   *     may take multiple hours. Multiple calls to `ObjectRewriter::Iterate()`
   *     may be required to completely rewrite an object.
   *
   * @param source_bucket_name the name of the bucket containing the source
   *     object.
   * @param source_object_name the name of the source object.
   * @param destination_bucket_name where the destination object will be
   *     located.
   * @param destination_object_name what to name the destination object.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationKmsKeyName`,
   *      `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *      `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *      `IfSourceGenerationMatch`, `IfSourceGenerationNotMatch`,
   *      `IfSourceMetagenerationMatch`, `IfSourceMetagenerationNotMatch`,
   *      `MaxBytesRewrittenPerCall`, `Projection`, `SourceEncryptionKey`,
   *      `SourceGeneration`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_rewrite_samples.cc rewrite object non blocking
   *
   * @par Example
   * @snippet storage_object_rewrite_samples.cc rewrite object resume
   */
  template <typename... Options>
  ObjectRewriter RewriteObject(std::string source_bucket_name,
                               std::string source_object_name,
                               std::string destination_bucket_name,
                               std::string destination_object_name,
                               Options&&... options) {
    return ResumeRewriteObject(
        std::move(source_bucket_name), std::move(source_object_name),
        std::move(destination_bucket_name), std::move(destination_object_name),
        std::string{}, std::forward<Options>(options)...);
  }

  /**
   * Creates an `ObjectRewriter` to resume a previously created rewrite.
   *
   * Applications use this function to resume a rewrite operation, possibly
   * created with `RewriteObject()`. Rewrite can reliably copy objects across
   * location boundaries, and can rewrite objects with different encryption
   * keys. For large objects this operation can take a long time, thus
   * applications should consider checkpointing the rewrite token (accessible in
   * the `ObjectRewriter`) and restarting the operation in the event the program
   * is terminated.
   *
   * @note Application developers should be aware that rewriting large objects
   *     may take multiple hours. Multiple calls to `ObjectRewriter::Iterate()`
   *     may be required to completely rewrite an object.
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
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationKmsKeyName`,
   *      `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *      `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *      `IfSourceGenerationMatch`, `IfSourceGenerationNotMatch`,
   *      `IfSourceMetagenerationMatch`, `IfSourceMetagenerationNotMatch`,
   *      `MaxBytesRewrittenPerCall`, `Projection`, `SourceEncryptionKey`,
   *      `SourceGeneration`, `UserProject`, and `WithObjectMetadata`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_rewrite_samples.cc rewrite object resume
   */
  template <typename... Options>
  ObjectRewriter ResumeRewriteObject(std::string source_bucket_name,
                                     std::string source_object_name,
                                     std::string destination_bucket_name,
                                     std::string destination_object_name,
                                     std::string rewrite_token,
                                     Options&&... options) {
    internal::RewriteObjectRequest request(
        std::move(source_bucket_name), std::move(source_object_name),
        std::move(destination_bucket_name), std::move(destination_object_name),
        std::move(rewrite_token));
    request.set_multiple_options(std::forward<Options>(options)...);
    return ObjectRewriter(raw_client_, std::move(request));
  }

  /**
   * Rewrites the object, blocking until the rewrite completes, and returns the
   * resulting `ObjectMetadata`.
   *
   * Applications use this function to reliably copy objects across [location
   * boundaries](https://cloud.google.com/storage/docs/locations), and to
   * rewrite objects with different encryption keys. The operation blocks until
   * the rewrite completes, and returns the resulting `ObjectMetadata`.
   *
   * @note Application developers should be aware that rewriting large objects
   *     may take multiple hours. In such cases the application should consider
   *     using `RewriteObject()` or `ResumeRewriteObject()`.
   *
   * @param source_bucket_name the name of the bucket containing the source
   *     object.
   * @param source_object_name the name of the source object.
   * @param destination_bucket_name where the destination object will be
   *     located.
   * @param destination_object_name what to name the destination object.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `DestinationKmsKeyName`,
   *      `DestinationPredefinedAcl`, `EncryptionKey`, `IfGenerationMatch`,
   *      `IfGenerationNotMatch`, `IfMetagenerationMatch`,
   *      `IfSourceGenerationMatch`, `IfSourceGenerationNotMatch`,
   *      `IfSourceMetagenerationMatch`, `IfSourceMetagenerationNotMatch`,
   *      `MaxBytesRewrittenPerCall`, `Projection`, `SourceEncryptionKey`,
   *      `SourceGeneration`, `UserProject`, and `WithObjectMetadata`.
   *
   * @return The metadata of the newly created object.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions, in this
   * case, `IfGenerationMatch`.
   *
   * @par Example
   * @snippet storage_object_rewrite_samples.cc rewrite object
   *
   * @par Example: using rewrite object to rotate the encryption key
   * @snippet storage_object_csek_samples.cc rotate encryption key
   *
   * @par Example: using rewrite object to rename an object
   * @snippet storage_object_rewrite_samples.cc rename object
   */
  template <typename... Options>
  StatusOr<ObjectMetadata> RewriteObjectBlocking(
      std::string source_bucket_name, std::string source_object_name,
      std::string destination_bucket_name, std::string destination_object_name,
      Options&&... options) {
    return ResumeRewriteObject(std::move(source_bucket_name),
                               std::move(source_object_name),
                               std::move(destination_bucket_name),
                               std::move(destination_object_name),
                               std::string{}, std::forward<Options>(options)...)
        .Result();
  }
  //@}

  //@{
  /**
   * @name Bucket Access Control List operations.
   *
   * You can control who has access to your GCS buckets and objects as
   * well as what level of access they have.
   *
   * @note In most cases, you should use IAM permissions instead of ACLs to
   * control access to buckets.
   *
   * @note All buckets are owned by the project owners group. Project owners
   *     are granted `OWNER` permissions to all buckets inside their project.
   *     Bucket and object ownership cannot be changed by modifying ACLs.
   *
   * @note When you apply a new ACL to a bucket, GCS adds `OWNER` permission to
   *     the bucket if you omit the grants. You cannot remove the project owners
   *     group from a bucket ACL. Attempts to do so will appear to succeed, but
   *     the service will add the project owners group into the new ACL before
   *     applying it.
   *
   * @see https://cloud.google.com/storage/docs/access-control/ for more
   *     information about access control in GCS.
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/lists#defaultbuckets
   *     for more details about the default owners for a bucket.
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameters.
   */
  /**
   * Retrieves the list of `BucketAccessControl` items for a bucket.
   *
   * @param bucket_name the name of the bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc list bucket acl
   */
  template <typename... Options>
  StatusOr<std::vector<BucketAccessControl>> ListBucketAcl(
      std::string const& bucket_name, Options&&... options) {
    internal::ListBucketAclRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto items = raw_client_->ListBucketAcl(request);
    if (!items) {
      return std::move(items).status();
    }
    return std::move(items.value().items);
  }

  /**
   * Creates a new entry in a bucket ACL.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param role the role of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc create bucket acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<BucketAccessControl> CreateBucketAcl(std::string const& bucket_name,
                                                std::string const& entity,
                                                std::string const& role,
                                                Options&&... options) {
    internal::CreateBucketAclRequest request(bucket_name, entity, role);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateBucketAcl(request);
  }

  /**
   * Deletes an entry from a bucket ACL.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc delete bucket acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   */
  template <typename... Options>
  Status DeleteBucketAcl(std::string const& bucket_name,
                         std::string const& entity, Options&&... options) {
    internal::DeleteBucketAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteBucketAcl(request).status();
  }

  /**
   * Gets the value of an existing bucket ACL.
   *
   * @param bucket_name the name of the bucket to query.
   * @param entity the name of the entity to query.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc get bucket acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   */
  template <typename... Options>
  StatusOr<BucketAccessControl> GetBucketAcl(std::string const& bucket_name,
                                             std::string const& entity,
                                             Options&&... options) {
    internal::GetBucketAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetBucketAcl(request);
  }

  /**
   * Updates the value of an existing bucket ACL.
   *
   * @note
   * For changing BucketAccessControl, the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param acl the new ACL value. Note that only the writable values of the ACL
   *   will be modified by the server.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc update bucket acl
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
   *     for additional details on what fields are writeable.
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<BucketAccessControl> UpdateBucketAcl(std::string const& bucket_name,
                                                BucketAccessControl const& acl,
                                                Options&&... options) {
    internal::UpdateBucketAclRequest request(bucket_name, acl.entity(),
                                             acl.role());
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateBucketAcl(request);
  }

  /**
   * Patches the value of an existing bucket ACL.
   *
   * Computes the delta between a previous value for an BucketAccessControl and
   * the new value for an BucketAccessControl and apply that delta.
   *
   * @note
   * For changing BucketAccessControl, the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param original_acl the original ACL value.
   * @param new_acl the new ACL value. Note that only changes on writeable
   *     fields will be accepted by the server.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`, and the standard
   *     options available to all operations.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc patch bucket acl
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
   *     for additional details on what fields are writeable.
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   */
  template <typename... Options>
  StatusOr<BucketAccessControl> PatchBucketAcl(
      std::string const& bucket_name, std::string const& entity,
      BucketAccessControl const& original_acl,
      BucketAccessControl const& new_acl, Options&&... options) {
    internal::PatchBucketAclRequest request(bucket_name, entity, original_acl,
                                            new_acl);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucketAcl(request);
  }

  /**
   * Patches the value of an existing bucket ACL.
   *
   * This API allows the application to patch an BucketAccessControl without
   * having to read the current value.
   *
   * @note
   * For changing BucketAccessControl, the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param builder a builder ready to create the patch.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `Generation`,
   *     `UserProject`, `IfMatchEtag`, and `IfNoneMatchEtag`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_bucket_acl_samples.cc patch bucket acl no-read
   *
   * @see https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
   *     for additional details on what fields are writeable.
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   */
  template <typename... Options>
  StatusOr<BucketAccessControl> PatchBucketAcl(
      std::string const& bucket_name, std::string const& entity,
      BucketAccessControlPatchBuilder const& builder, Options&&... options) {
    internal::PatchBucketAclRequest request(bucket_name, entity, builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchBucketAcl(request);
  }
  //@}

  //@{
  /**
   * @name Object Access Control List operations.
   *
   * You can control who has access to your GCS buckets and objects as
   * well as what level of access they have.
   *
   * @note When you upload (create) an object the entity that uploads the object
   *     is listed as the object owner. This can be a user or a service account,
   *     depending on what credentials are used to authenticate with GCS.
   *     Object ownership cannot be changed by modifying ACLs. You can change
   *     object ownership only by overwriting the object while authenticated as
   *     the desired owner.
   *
   * @note When you apply a new ACL to an object, GCS adds `OWNER` permission to
   *     the object if you omit the grants.
   *
   * @see https://cloud.google.com/storage/docs/access-control/ for more
   *     information about access control in GCS.
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameters.
   */
  /**
   * Retrieves the list of ObjectAccessControl items for an object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc list object acl
   */
  template <typename... Options>
  StatusOr<std::vector<ObjectAccessControl>> ListObjectAcl(
      std::string const& bucket_name, std::string const& object_name,
      Options&&... options) {
    internal::ListObjectAclRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto result = raw_client_->ListObjectAcl(request);
    if (!result) {
      return std::move(result).status();
    }
    return std::move(result.value().items);
  }

  /**
   * Creates a new entry in the object ACL.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the name of the entity added to the ACL.
   * @param role the role of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc create object acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> CreateObjectAcl(std::string const& bucket_name,
                                                std::string const& object_name,
                                                std::string const& entity,
                                                std::string const& role,
                                                Options&&... options) {
    internal::CreateObjectAclRequest request(bucket_name, object_name, entity,
                                             role);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateObjectAcl(request);
  }

  /**
   * Deletes one access control entry in one object.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object to be deleted.
   * @param entity the name of the entity (user, team, group) to be removed from
   *   the Object's ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc delete object acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   */
  template <typename... Options>
  Status DeleteObjectAcl(std::string const& bucket_name,
                         std::string const& object_name,
                         std::string const& entity, Options&&... options) {
    internal::DeleteObjectAclRequest request(bucket_name, object_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteObjectAcl(request).status();
  }

  /**
   * Gets the value of an existing object ACL.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the name of the entity added to the ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc print file acl for user
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> GetObjectAcl(std::string const& bucket_name,
                                             std::string const& object_name,
                                             std::string const& entity,
                                             Options&&... options) {
    internal::GetObjectAclRequest request(bucket_name, object_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetObjectAcl(request);
  }

  /**
   * Updates the value of an existing object ACL.
   *
   * @note
   * For changing ObjectAccessControl, the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param acl the new ACL value. Note that only the writable values of the ACL
   *   will be modified by the server.
   * @param options a list of optional query parameters and/or request
   *     Valid types for this operation include `Generation`, and `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc update object acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   * @see https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> UpdateObjectAcl(std::string const& bucket_name,
                                                std::string const& object_name,
                                                ObjectAccessControl const& acl,
                                                Options&&... options) {
    internal::UpdateObjectAclRequest request(bucket_name, object_name,
                                             acl.entity(), acl.role());
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateObjectAcl(request);
  }

  /**
   * Patches the value of an existing object ACL.
   *
   * Compute the delta between a previous value for an ObjectAccessControl and
   * the new value for an ObjectAccessControl and apply that delta.
   *
   * @note
   * For changing ObjectAccessControl, the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param original_acl the original ACL value.
   * @param new_acl the new ACL value. Note that only changes on writeable
   *     fields will be accepted by the server.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `Generation`,
   *     `UserProject`, `IfMatchEtag`, and `IfNoneMatchEtag`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc patch object acl
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   * @see https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      std::string const& bucket_name, std::string const& object_name,
      std::string const& entity, ObjectAccessControl const& original_acl,
      ObjectAccessControl const& new_acl, Options&&... options) {
    internal::PatchObjectAclRequest request(bucket_name, object_name, entity,
                                            original_acl, new_acl);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchObjectAcl(request);
  }

  /**
   * Patches the value of an existing object ACL.
   *
   * This API allows the application to patch an ObjectAccessControl without
   * having to read the current value.
   *
   * @note
   * For changing ObjectAccessControl, the Patch and Update APIs basically offer
   * the same functionality. The only field that can be modified by either API
   * is `role`, and it may only be set to a new value (it cannot be removed).
   * The API is offered for consistency with the other resource types where
   * Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket that contains the object.
   * @param object_name the name of the object.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param builder a builder ready to create the patch.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `Generation`,
   *     `UserProject`, `IfMatchEtag`, and `IfNoneMatchEtag`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_object_acl_samples.cc patch object acl no-read
   *
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   * @see https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
   *     for additional details on what fields are writeable.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      std::string const& bucket_name, std::string const& object_name,
      std::string const& entity, ObjectAccessControlPatchBuilder const& builder,
      Options&&... options) {
    internal::PatchObjectAclRequest request(bucket_name, object_name, entity,
                                            builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchObjectAcl(request);
  }
  //@}

  //@{
  /**
   * @name Bucket Default Object Access Control List operations.
   *
   * When you upload an object to GCS without specifying an ACL, the object is
   * created with the Default Object ACL for its bucket. These operations are
   * used to query and modify the Default Object ACL of a bucket.
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/lists#defaultobjects
   *     for more information on default object ACLs.
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameters.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameters.
   */
  /**
   * Retrieves the default object ACL for a bucket as a vector of
   * `ObjectAccessControl` items.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `IfMetagenerationMatch`,
   *     `IfMetagenerationNotMatch` and `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc list default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   */
  template <typename... Options>
  StatusOr<std::vector<ObjectAccessControl>> ListDefaultObjectAcl(
      std::string const& bucket_name, Options&&... options) {
    internal::ListDefaultObjectAclRequest request(bucket_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto response = raw_client_->ListDefaultObjectAcl(request);
    if (!response) {
      return std::move(response).status();
    }
    return std::move(response.value().items);
  }

  /**
   * Creates a new entry in the default object ACL for a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param role the role of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc create default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      std::string const& bucket_name, std::string const& entity,
      std::string const& role, Options&&... options) {
    internal::CreateDefaultObjectAclRequest request(bucket_name, entity, role);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->CreateDefaultObjectAcl(request);
  }

  /**
   * Deletes an entry from the default object ACL in a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity added to the ACL.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc delete default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   */
  template <typename... Options>
  Status DeleteDefaultObjectAcl(std::string const& bucket_name,
                                std::string const& entity,
                                Options&&... options) {
    internal::DeleteDefaultObjectAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteDefaultObjectAcl(request).status();
  }

  /**
   * Gets the value of a default object ACL in a bucket.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the name of the entity.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc get default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      std::string const& bucket_name, std::string const& entity,
      Options&&... options) {
    internal::GetDefaultObjectAclRequest request(bucket_name, entity);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetDefaultObjectAcl(request);
  }

  /**
   * Updates the value of an existing default object ACL.
   *
   * The default object ACL sets the ACL for any object created in the bucket,
   * unless a different ACL is specified when the object is created.
   *
   * @note
   * For changing default object access controls, the Patch and Update APIs
   * basically offer the same functionality. The only field that can be modified
   * by either API is `role`, and it may only be set to a new value (it cannot
   * be removed). The API is offered for consistency with the other resource
   * types where Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param acl the new ACL value. Note that only the writable values of the ACL
   *   will be modified by the server.
   * @param options a list of optional query parameters and/or request
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc update default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      std::string const& bucket_name, ObjectAccessControl const& acl,
      Options&&... options) {
    internal::UpdateDefaultObjectAclRequest request(bucket_name, acl.entity(),
                                                    acl.role());
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateDefaultObjectAcl(request);
  }

  /**
   * Patches the value of an existing default object ACL.
   *
   * Compute the delta between a previous and new values for a default object
   * access control, and apply that delta.
   *
   * @note
   * For changing default object access controls, the Patch and Update APIs
   * basically offer the same functionality. The only field that can be modified
   * by either API is `role`, and it may only be set to a new value (it cannot
   * be removed). The API is offered for consistency with the other resource
   * types where Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param original_acl the original ACL value.
   * @param new_acl the new ACL value. Note that only changes on writeable
   *     fields will be accepted by the server.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `UserProject`, as well
   *     as the standard parameters, such as `IfMatchEtag`, and
   *     `IfNoneMatchEtag`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc patch default object acl
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      std::string const& bucket_name, std::string const& entity,
      ObjectAccessControl const& original_acl,
      ObjectAccessControl const& new_acl, Options&&... options) {
    internal::PatchDefaultObjectAclRequest request(bucket_name, entity,
                                                   original_acl, new_acl);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchDefaultObjectAcl(request);
  }

  /**
   * Patches the value of an existing default object ACL.
   *
   * This API allows the application to patch an ObjectAccessControl without
   * having to read the current value.
   *
   * @note
   * For changing default object access controls, the Patch and Update APIs
   * basically offer the same functionality. The only field that can be modified
   * by either API is `role`, and it may only be set to a new value (it cannot
   * be removed). The API is offered for consistency with the other resource
   * types where Patch and Update APIs have different semantics.
   *
   * @param bucket_name the name of the bucket.
   * @param entity the identifier for the user, group, service account, or
   *     predefined set of actors holding the permission.
   * @param builder a builder ready to create the patch.
   * @param options a list of optional query parameters and/or request
   *     headers. Valid types for this operation include `UserProject`, as well
   *     as the standard parameters, such as `IfMatchEtag`, and
   *     `IfNoneMatchEtag`.
   *
   * @par Idempotency
   * This operation is only idempotent if restricted by pre-conditions. There
   * are no pre-conditions for this operation that can make it idempotent.
   *
   * @par Example
   * @snippet storage_default_object_acl_samples.cc patch default object acl
   * no-read
   *
   * @see
   * https://cloud.google.com/storage/docs/access-control/create-manage-lists#defaultobjects
   * @see https://cloud.google.com/storage/docs/access-control/lists#scopes for
   *     the format of the @p entity parameter.
   * @see https://cloud.google.com/storage/docs/access-control/lists#permissions
   *     for the format of the @p role parameter.
   */
  template <typename... Options>
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      std::string const& bucket_name, std::string const& entity,
      ObjectAccessControlPatchBuilder const& builder, Options&&... options) {
    internal::PatchDefaultObjectAclRequest request(bucket_name, entity,
                                                   builder);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->PatchDefaultObjectAcl(request);
  }
  //@}

  //@{
  /**
   * @name Service account operations.
   *
   * Service accounts allow applications to authenticate and access GCP
   * resources and services. When acting on your behalf, GCS uses such a service
   * account. GCS creates one service account per project. These operations
   * allow you to query the GCS service account for a project.
   *
   * @see https://cloud.google.com/storage/docs/projects#service-accounts for
   *     more information on service accounts.
   */
  /**
   * Gets the GCS service account for a given project.
   *
   * A service account is a special Google account that belongs to your
   * application, virtual machine, or to a Google service when acting on your
   * behalf.  This API allows you to discover the GCS service account for the
   * @p project_id project.
   *
   * @param project_id the project to query.
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc get service account for project
   *
   * @see https://cloud.google.com/iam/docs/service-accounts for general
   *     information on Google Cloud Platform service accounts.
   */
  template <typename... Options>
  StatusOr<ServiceAccount> GetServiceAccountForProject(
      std::string const& project_id, Options&&... options) {
    internal::GetProjectServiceAccountRequest request(project_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetServiceAccount(request);
  }

  /**
   * Gets the GCS service account for the default project.
   *
   * A service account is a special Google account that belongs to your
   * application, virtual machine, or to a Google service when acting on your
   * behalf.  This API allows you to discover the GCS service account for the
   * default project associated with this object.
   *
   * The default project is required to be configured in the `ClientOptions`
   * used to construct this object. If the application does not set the project
   * id in the `ClientOptions`, the value of the `GOOGLE_CLOUD_PROJECT` is
   * used. If neither the environment variable is set, nor a value is set
   * explicitly by the application, the server will reject the request and this
   * function will return the error status.
   *
   * @param options a list of optional query parameters and/or request headers.
   *     Valid types for this operation include `UserProject`.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc get service account
   *
   * @see https://cloud.google.com/iam/docs/service-accounts for general
   *     information on Google Cloud Platform service accounts.
   */
  template <typename... Options>
  StatusOr<ServiceAccount> GetServiceAccount(Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    return GetServiceAccountForProject(project_id,
                                       std::forward<Options>(options)...);
  }

  /**
   * List the available HMAC keys.
   *
   * @warning This GCS feature is not GA, it is subject to change without
   *     notice.
   *
   * @param options a list of optional query parameters and/or request headers.
   *     In addition to the options common to all requests, this operation
   *     accepts `Deleted` `MaxResults`, `OverrideDefaultProject`,
   *     `ServiceAccountFilter`, and `UserProject`.
   *
   * @return A range to iterate over the available HMAC keys.
   *
   * @par Idempotency
   * This is a read-only operation and is always idempotent.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc list hmac keys
   *
   * @par Example
   * @snippet storage_service_account_samples.cc list hmac keys service account
   *
   * @see https://cloud.google.com/storage/docs/authentication/hmackeys for
   *     general information on using HMAC keys for Google Cloud Platform
   *     service accounts authentication.
   *
   * @see https://cloud.google.com/storage/docs/authentication/managing-hmackeys
   *     for a detailed description on how to use the feature.
   */
  template <typename... Options>
  ListHmacKeysReader ListHmacKeys(Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    internal::ListHmacKeysRequest request(project_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    auto client = raw_client_;
    return google::cloud::internal::MakePaginationRange<ListHmacKeysReader>(
        request,
        [client](internal::ListHmacKeysRequest const& r) {
          return client->ListHmacKeys(r);
        },
        [](internal::ListHmacKeysResponse r) { return std::move(r.items); });
  }

  /**
   * Create a new HMAC key.
   *
   * @warning This GCS feature is not GA, it is subject to change without
   *     notice.
   *
   * @param service_account the service account email where you want to create
   *     the new HMAC key.
   * @param options a list of optional query parameters and/or request headers.
   *     In addition to the options common to all requests, this operation
   *     accepts `OverrideDefaultProject`.
   *
   * @return This operation returns the new HMAC key metadata *and* the HMAC key
   *   secret (encoded as a base64 string). This is the only request that
   *   returns the secret.
   *
   * @par Idempotency
   * This operation is not idempotent. Retrying the operation will create a new
   * key each time.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc create hmac key
   *
   * @par Example
   * @snippet storage_service_account_samples.cc create hmac key project
   *
   * @see https://cloud.google.com/storage/docs/authentication/hmackeys for
   *     general information on using HMAC keys for Google Cloud Platform
   *     service accounts authentication.
   *
   * @see https://cloud.google.com/storage/docs/authentication/managing-hmackeys
   *     for a detailed description on how to use the feature.
   */
  template <typename... Options>
  StatusOr<std::pair<HmacKeyMetadata, std::string>> CreateHmacKey(
      std::string service_account, Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    internal::CreateHmacKeyRequest request(project_id,
                                           std::move(service_account));
    request.set_multiple_options(std::forward<Options>(options)...);
    auto result = raw_client_->CreateHmacKey(request);
    if (!result) {
      return result.status();
    }
    return std::make_pair(std::move(result->metadata),
                          std::move(result->secret));
  }

  /**
   * Delete a HMAC key in a given project.
   *
   * @warning This GCS feature is not GA, it is subject to change without
   *     notice.
   *
   * @param access_id the HMAC key `access_id()` that you want to delete.  Each
   *     HMAC key is assigned an `access_id()` attribute at creation time.
   * @param options a list of optional query parameters and/or request headers.
   *     In addition to the options common to all requests, this operation
   *     accepts `OverrideDefaultProject`.
   *
   * @return This operation returns the new HMAC key metadata.
   *
   * @par Idempotency
   * This operation is always idempotent. An access id identifies a single HMAC
   * key, calling the operation multiple times can succeed only once.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc delete hmac key
   *
   * @see https://cloud.google.com/storage/docs/authentication/hmackeys for
   *     general information on using HMAC keys for Google Cloud Platform
   *     service accounts authentication.
   *
   * @see https://cloud.google.com/storage/docs/authentication/managing-hmackeys
   *     for a detailed description on how to use the feature.
   */
  template <typename... Options>
  Status DeleteHmacKey(std::string access_id, Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    internal::DeleteHmacKeyRequest request(project_id, std::move(access_id));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->DeleteHmacKey(request).status();
  }

  /**
   * Get an existing HMAC key in a given project.
   *
   * @warning This GCS feature is not GA, it is subject to change without
   *     notice.
   *
   * @param access_id the HMAC key `access_id()` that you want to delete.  Each
   *     HMAC key is assigned an `access_id()` attribute at creation time.
   * @param options a list of optional query parameters and/or request headers.
   *     In addition to the options common to all requests, this operation
   *     accepts `OverrideDefaultProject`.
   *
   * @return This operation returns the new HMAC key metadata.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always idempotent.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc get hmac key
   *
   * @see https://cloud.google.com/storage/docs/authentication/hmackeys for
   *     general information on using HMAC keys for Google Cloud Platform
   *     service accounts authentication.
   *
   * @see https://cloud.google.com/storage/docs/authentication/managing-hmackeys
   *     for a detailed description on how to use the feature.
   */
  template <typename... Options>
  StatusOr<HmacKeyMetadata> GetHmacKey(std::string access_id,
                                       Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    internal::GetHmacKeyRequest request(project_id, std::move(access_id));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->GetHmacKey(request);
  }

  /**
   * Update an existing HMAC key in a given project.
   *
   * @warning This GCS feature is not GA, it is subject to change without
   *     notice.
   *
   * @param access_id the HMAC key `access_id()` that you want to delete.  Each
   *     HMAC key is assigned an `access_id()` attribute at creation time.
   * @param resource the desired changes to the HMAC key resource. Only the
   *     `state` field may be changed. The `etag` field may be set but it is
   *     only used as a pre-condition, the application cannot set the `etag`.
   * @param options a list of optional query parameters and/or request headers.
   *     In addition to the options common to all requests, this operation
   *     accepts `OverrideDefaultProject`.
   *
   * @return This operation returns the new HMAC key metadata.
   *
   * @par Idempotency
   * This operation is only idempotent if the `etag` attribute in @p resource
   * is set, or if the `IfMatchEtag` option is set.
   *
   * @par Example
   * @snippet storage_service_account_samples.cc update hmac key
   *
   * @see https://cloud.google.com/storage/docs/authentication/hmackeys for
   *     general information on using HMAC keys for Google Cloud Platform
   *     service accounts authentication.
   *
   * @see https://cloud.google.com/storage/docs/authentication/managing-hmackeys
   *     for a detailed description on how to use the feature.
   */
  template <typename... Options>
  StatusOr<HmacKeyMetadata> UpdateHmacKey(std::string access_id,
                                          HmacKeyMetadata resource,
                                          Options&&... options) {
    auto const& project_id = raw_client_->client_options().project_id();
    internal::UpdateHmacKeyRequest request(project_id, std::move(access_id),
                                           std::move(resource));
    request.set_multiple_options(std::forward<Options>(options)...);
    return raw_client_->UpdateHmacKey(request);
  }
  //@}

  //@{
  /// @name Signed URL support operations.
  /**
   * Create a V2 signed URL for the given parameters.
   *
   * @note By default URLs created with this function expire after 7 days.
   *
   * @note The application must ensure that any URL created with this function
   *     is a valid request via the XML API. For example, the options for
   *     bucket requests may include a sub-resource (e.g. `WithBilling()`) but
   *     not all sub-resources are valid for objects.  Likewise, only a single
   *     sub-resource may be retrieved in each request.
   *
   * @param verb the operation allowed through this signed URL, `GET`, `POST`,
   *     `PUT`, `HEAD`, etc. are valid values.
   * @param bucket_name the name of the bucket.
   * @param object_name the name of the object, note that the object may not
   *     exist for signed URLs that upload new objects. Use an empty string for
   *     requests that only affect a bucket.
   * @param options a list of optional parameters for the signed URL, this
   *     include: `ExpirationTime`, `MD5HashValue`, `ContentType`,
   *     `SigningAccount`, `SigningAccountDelegates`,
   *     `AddExtensionHeaderOption`, `AddQueryParameterOption`, and
   *     `AddSubResourceOption`. Note that only the last `AddSubResourceOption`
   *     option has any effect.
   *
   * @par Helper Functions
   *
   * The following functions create a `AddSubResourceOption` with less
   * opportunities for typos in the sub-resource name: `WithAcl()`,
   * `WithBilling()`, `WithCompose()`, `WithCors()`, `WithEncryption()`,
   * `WithEncryptionConfig()`, `WithLifecycle()`, `WithLocation()`,
   * `WithLogging()`, `WithStorageClass()`, and `WithTagging()`.
   *
   * Likewise, the following helper functions can create properly formatted
   * `AddExtensionHeaderOption` objects: `WithGeneration()`,
   * `WithGenerationMarker()`, `WithMarker()`,
   * `WithResponseContentDisposition()`, `WithResponseContentType()`, and
   * `WithUserProject()`.
   *
   * @return the signed URL.
   *
   * @par Example
   * @snippet storage_signed_url_v2_samples.cc sign url v2
   *
   * @par Example
   * @snippet storage_signed_url_v2_samples.cc create put signed url v2
   *
   * @see https://cloud.google.com/storage/docs/access-control/signed-urls for
   *     a general description of signed URLs and how they can be used.
   *
   * @see https://cloud.google.com/storage/docs/xml-api/overview for a detailed
   *     description of the XML API.
   */
  template <typename... Options>
  StatusOr<std::string> CreateV2SignedUrl(std::string verb,
                                          std::string bucket_name,
                                          std::string object_name,
                                          Options&&... options) {
    internal::V2SignUrlRequest request(std::move(verb), std::move(bucket_name),
                                       std::move(object_name));
    request.set_multiple_options(std::forward<Options>(options)...);
    return SignUrlV2(request);
  }

  /**
   * Create a V4 signed URL for the given parameters.
   *
   * @note By default URLs created with this function expire after 7 days.
   *
   * @note The application must ensure that any URL created with this function
   *     is a valid request via the XML API. For example, the options for
   *     bucket requests may include a sub-resource (e.g. `WithBilling()`) but
   *     not all sub-resources are valid for objects.  Likewise, only a single
   *     sub-resource may be retrieved in each request.
   *
   * @param verb the operation allowed through this signed URL, `GET`, `POST`,
   *     `PUT`, `HEAD`, etc. are valid values.
   * @param bucket_name the name of the bucket.
   * @param object_name the name of the object, note that the object may not
   *     exist for signed URLs that upload new objects. Use an empty string for
   *     requests that only affect a bucket.
   * @param options a list of optional parameters for the signed URL, this
   *     include: `SignedUrlTimestamp`, `SignedUrlDuration`, `MD5HashValue`,
   *     `ContentType`, `SigningAccount`, `SigningAccountDelegates`,
   *     `AddExtensionHeaderOption`, `AddQueryParameterOption`, and
   *     `AddSubResourceOption`. Note that only the last `AddSubResourceOption`
   *     option has any effect.
   *
   * @par Helper Functions
   *
   * The following functions create a `AddSubResourceOption` with less
   * opportunities for typos in the sub-resource name: `WithAcl()`,
   * `WithBilling()`, `WithCompose()`, `WithCors()`, `WithEncryption()`,
   * `WithEncryptionConfig()`, `WithLifecycle()`, `WithLocation()`,
   * `WithLogging()`, `WithStorageClass()`, and `WithTagging()`.
   *
   * Likewise, the following helper functions can create properly formatted
   * `AddExtensionHeaderOption` objects: `WithGeneration()`,
   * `WithGenerationMarker()`, `WithMarker()`,
   * `WithResponseContentDisposition()`, `WithResponseContentType()`, and
   * `WithUserProject()`.
   *
   * @return the signed URL.
   *
   * @par Example
   * @snippet storage_signed_url_v4_samples.cc sign url v4
   *
   * @par Example
   * @snippet storage_signed_url_v4_samples.cc create put signed url v4
   *
   * @see https://cloud.google.com/storage/docs/access-control/signed-urls for
   *     a general description of signed URLs and how they can be used.
   *
   * @see https://cloud.google.com/storage/docs/xml-api/overview for a detailed
   *     description of the XML API.
   */
  template <typename... Options>
  StatusOr<std::string> CreateV4SignedUrl(std::string verb,
                                          std::string bucket_name,
                                          std::string object_name,
                                          Options&&... options) {
    internal::V4SignUrlRequest request(std::move(verb), std::move(bucket_name),
                                       std::move(object_name));
    request.set_multiple_options(std::forward<Options>(options)...);
    return SignUrlV4(std::move(request));
  }
  //@}

  /**
   * Create a signed policy document.
   *
   * @note The application must ensure that any document created with this
   * function contains valid conditions. This function does not do any error
   * checking, e.g. that a `ExactMatchObject()` condition contains two
   * elements. Using the provided helper functions can prevent errors.
   *
   * @note It is the application's responsibility to construct a POST request
   * based on the value returned by this function.  For example, a web
   * application can create a HTML form containing these fields, the result of
   * which is a POST request to GCS.
   *
   * @param document the policy document.
   * @param options a list of optional parameters, this includes:
   *      `SigningAccount`, and `SigningAccountDelegates`.
   *
   * @par Helper Functions
   * The following functions create a `PolicyDocumentCondition` with less
   * opportunities for typos: `StartsWith()`, `ExactMatchObject()`,
   * `ExactMatch()`, `ContentLengthRange()`.
   *
   * @par Example
   * @snippet storage_policy_doc_samples.cc create signed policy document
   *
   * @see
   * https://cloud.google.com/storage/docs/xml-api/post-object#policydocument
   *     for a general description of policy documents and how they can be used.
   *
   * @see https://cloud.google.com/storage/docs/xml-api/overview for a detailed
   *     description of the XML API.
   */
  template <typename... Options>
  StatusOr<PolicyDocumentResult> CreateSignedPolicyDocument(
      PolicyDocument document, Options&&... options) {
    internal::PolicyDocumentRequest request(std::move(document));
    request.set_multiple_options(std::forward<Options>(options)...);
    return SignPolicyDocument(request);
  }

  /**
   * Create a signed V4 policy document.
   *
   * @note The application must ensure that any document created with this
   * function contains valid conditions. This function does not do any error
   * checking, e.g. that a `ExactMatchObject()` condition contains two
   * elements. Using the provided helper functions can prevent errors.
   *
   * @note It is the application's responsibility to construct a POST request
   * based on the value returned by this function.  For example, a web
   * application can create a HTML form containing these fields, the result of
   * which is a POST request to GCS.
   *
   * @param document the policy document.
   * @param options a list of optional parameters, this includes:
   * `AddExtensionFieldOption`, `BucketBoundHostname`, `PredefinedAcl`,
   * `Scheme`, `SigningAccountDelegates`, `SigningAccount`, `VirtualHostname`
   *
   * @par Helper Functions
   * The following functions create a `PolicyDocumentCondition` with less
   * opportunities for typos: `StartsWith()`, `ExactMatchObject()`,
   * `ExactMatch()`, `ContentLengthRange()`.
   *
   * @par Example
   * @snippet storage_policy_doc_samples.cc create signed policy document v4
   *
   * @see
   * https://cloud.google.com/storage/docs/xml-api/post-object#policydocument
   *     for a general description of policy documents and how they can be used.
   *
   * @see https://cloud.google.com/storage/docs/xml-api/overview for a detailed
   *     description of the XML API.
   */
  template <typename... Options>
  StatusOr<PolicyDocumentV4Result> GenerateSignedPostPolicyV4(
      PolicyDocumentV4 document, Options&&... options) {
    internal::PolicyDocumentV4Request request(std::move(document));
    request.set_multiple_options(std::forward<Options>(options)...);
    return SignPolicyDocumentV4(std::move(request));
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
    if (!result) {
      return std::move(result).status();
    }
    return std::move(result).value().items;
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
    return raw_client_->CreateNotification(request);
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
    return raw_client_->GetNotification(request);
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
  Status DeleteNotification(std::string const& bucket_name,
                            std::string const& notification_id,
                            Options&&... options) {
    internal::DeleteNotificationRequest request(bucket_name, notification_id);
    request.set_multiple_options(std::forward<Options>(options)...);
    return std::move(raw_client_->DeleteNotification(request)).status();
  }
  //@}

 private:
  Client() = default;
  static std::shared_ptr<internal::RawClient> CreateDefaultInternalClient(
      ClientOptions options);

  template <typename... Policies>
  std::shared_ptr<internal::RawClient> Decorate(
      std::shared_ptr<internal::RawClient> client, Policies&&... policies) {
    if (client->client_options().enable_raw_client_tracing()) {
      client = std::make_shared<internal::LoggingClient>(std::move(client));
    }
    auto retry = std::make_shared<internal::RetryClient>(
        std::move(client), std::forward<Policies>(policies)...);
    return retry;
  }

  ObjectReadStream ReadObjectImpl(
      internal::ReadObjectRangeRequest const& request);

  ObjectWriteStream WriteObjectImpl(
      internal::ResumableUploadRequest const& request);

  // The version of UploadFile() where UseResumableUploadSession is one of the
  // options. Note how this does not use InsertObjectMedia at all.
  template <typename... Options>
  StatusOr<ObjectMetadata> UploadFileImpl(std::string const& file_name,
                                          std::string const& bucket_name,
                                          std::string const& object_name,
                                          std::true_type,
                                          Options&&... options) {
    internal::ResumableUploadRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return UploadFileResumable(file_name, std::move(request));
  }

  // The version of UploadFile() where UseResumableUploadSession is *not* one of
  // the options. In this case we can use InsertObjectMediaRequest because it
  // is safe.
  template <typename... Options>
  StatusOr<ObjectMetadata> UploadFileImpl(std::string const& file_name,
                                          std::string const& bucket_name,
                                          std::string const& object_name,
                                          std::false_type,
                                          Options&&... options) {
    std::size_t file_size = 0;
    if (UseSimpleUpload(file_name, file_size)) {
      internal::InsertObjectMediaRequest request(bucket_name, object_name,
                                                 std::string{});
      request.set_multiple_options(std::forward<Options>(options)...);
      return UploadFileSimple(file_name, file_size, request);
    }
    internal::ResumableUploadRequest request(bucket_name, object_name);
    request.set_multiple_options(std::forward<Options>(options)...);
    return UploadFileResumable(file_name, std::move(request));
  }

  bool UseSimpleUpload(std::string const& file_name, std::size_t& size) const;

  StatusOr<ObjectMetadata> UploadFileSimple(
      std::string const& file_name, std::size_t file_size,
      internal::InsertObjectMediaRequest request);

  StatusOr<ObjectMetadata> UploadFileResumable(
      std::string const& file_name, internal::ResumableUploadRequest request);

  StatusOr<ObjectMetadata> UploadStreamResumable(
      std::istream& source, internal::ResumableUploadRequest const& request);

  Status DownloadFileImpl(internal::ReadObjectRangeRequest const& request,
                          std::string const& file_name);

  /// Determine the email used to sign a blob.
  std::string SigningEmail(SigningAccount const& signing_account);

  /// Represents the result of signing a blob, including the key used in the
  /// signature.
  struct SignBlobResponseRaw {
    std::string key_id;
    std::vector<std::uint8_t> signed_blob;
  };

  /// Sign a blob.
  StatusOr<SignBlobResponseRaw> SignBlobImpl(
      SigningAccount const& signing_account, std::string const& string_to_sign);

  StatusOr<std::string> SignUrlV2(internal::V2SignUrlRequest const& request);
  StatusOr<std::string> SignUrlV4(internal::V4SignUrlRequest request);

  StatusOr<PolicyDocumentResult> SignPolicyDocument(
      internal::PolicyDocumentRequest const& request);
  StatusOr<PolicyDocumentV4Result> SignPolicyDocumentV4(
      internal::PolicyDocumentV4Request request);

  std::shared_ptr<internal::RawClient> raw_client_;

  friend class internal::NonResumableParallelUploadState;
  friend class internal::ResumableParallelUploadState;
  friend class testing::ClientTester;
};

/**
 * Create a random prefix for object names.
 *
 * This is useful for operations which require a unique prefix for temporary
 * files.
 *
 * This is a helper function and it doesn't communicate with GCS, so there is a
 * very small chance that names generated this way aren't unique. The chance
 * should be small enough to fit any error budget.
 *
 * Given the same `prefix`, the randomly generated names will have the same
 * length.
 *
 * @param prefix the prefix of the prefix to be created.
 * @return the random prefix name
 */
std::string CreateRandomPrefixName(std::string const& prefix = "");

namespace internal {

// Just a wrapper to allow for using in `google::cloud::internal::apply`.
struct DeleteApplyHelper {
  template <typename... Options>
  Status operator()(Options... options) const {
    return client.DeleteObject(bucket_name, object_name, std::move(options)...);
  }

  Client& client;
  std::string bucket_name;
  std::string object_name;
};

// Just a wrapper to allow for using in `google::cloud::internal::apply`.
struct InsertObjectApplyHelper {
  template <typename... Options>
  StatusOr<ObjectMetadata> operator()(Options... options) const {
    return client.InsertObject(bucket_name, object_name, std::move(contents),
                               std::move(options)...);
  }

  Client& client;
  std::string const& bucket_name;
  std::string const& object_name;
  std::string contents;
};

/**
 * Create a "marker" object to ensure that two tasks cannot share a prefix.
 *
 * @param client the client on which to perform the operation.
 * @param bucket_name the name of the bucket that will contain the object.
 * @param prefix the prefix of the objects to be deleted.
 * @param options a list of optional query parameters and/or request headers.
 *      Valid types for this operation include `EncryptionKey` `KmsKeyName`,
 *     `PredefinedAcl`, `Projection`, `UserProject`, and `WithObjectMetadata`.
 *     Contrary to the public API, invalid options will be silently ignored
 *     for ease of use.
 * @return the metadata of the marker
 */
template <typename... Options>
StatusOr<ObjectMetadata> LockPrefix(Client& client,
                                    std::string const& bucket_name,
                                    std::string const& prefix,
                                    Options&&... options) {
  return google::cloud::internal::apply(
      internal::InsertObjectApplyHelper{client, bucket_name, prefix, ""},
      std::tuple_cat(
          std::make_tuple(IfGenerationMatch(0)),
          internal::StaticTupleFilter<
              internal::Among<EncryptionKey, KmsKeyName, PredefinedAcl,
                              Projection, UserProject>::TPred>(
              std::forward_as_tuple(std::forward<Options>(options)...))));
}

}  // namespace internal

/**
 * Delete objects whose names match a given prefix
 *
 * @param client the client on which to perform the operation.
 * @param bucket_name the name of the bucket that will contain the object.
 * @param prefix the prefix of the objects to be deleted.
 * @param options a list of optional query parameters and/or request headers.
 *     Valid types for this operation include `QuotaUser`, `UserIp`,
 *     `UserProject` and `Versions`.
 */
template <typename... Options>
Status DeleteByPrefix(Client& client, std::string const& bucket_name,
                      std::string const& prefix, Options&&... options) {
  using internal::NotAmong;
  using internal::StaticTupleFilter;

  auto all_options = std::tie(options...);

  static_assert(
      std::tuple_size<decltype(
              StaticTupleFilter<
                  NotAmong<QuotaUser, UserIp, UserProject, Versions>::TPred>(
                  all_options))>::value == 0,
      "This functions accepts only options of type QuotaUser, UserIp, "
      "UserProject or Versions.");
  for (auto const& object :
       client.ListObjects(bucket_name, Projection::NoAcl(), Prefix(prefix),
                          std::forward<Options>(options)...)) {
    if (!object) {
      return object.status();
    }

    auto deletion_status = google::cloud::internal::apply(
        internal::DeleteApplyHelper{client, bucket_name, object->name()},
        std::tuple_cat(
            std::make_tuple(IfGenerationMatch(object->generation())),
            StaticTupleFilter<NotAmong<Versions>::TPred>(all_options)));

    if (!deletion_status.ok()) {
      return deletion_status;
    }
  }
  return Status();
}

namespace internal {

// Just a wrapper to allow for use in `google::cloud::internal::apply`.
struct ComposeApplyHelper {
  template <typename... Options>
  StatusOr<ObjectMetadata> operator()(Options... options) const {
    return client.ComposeObject(
        std::move(bucket_name), std::move(source_objects),
        std::move(destination_object_name), std::move(options)...);
  }

  Client& client;
  std::string bucket_name;
  std::vector<ComposeSourceObject> source_objects;
  std::string destination_object_name;
};

// A helper to defer deletion of temporary GCS objects.
class ScopedDeleter {
 public:
  // The actual deletion depends on local's types in a very non-trivial way,
  // so we abstract this away by providing the function to delete one object.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ScopedDeleter(std::function<Status(std::string, std::int64_t)> delete_fun);
  ScopedDeleter(ScopedDeleter const&) = delete;
  ScopedDeleter& operator=(ScopedDeleter const&) = delete;
  ~ScopedDeleter();

  /// Defer object's deletion to this objects destruction (or ExecuteDelete())
  void Add(ObjectMetadata const& object);

  /// Defer object's deletion to this objects destruction (or ExecuteDelete())
  void Add(std::string object_name, std::int64_t generation);

  /// Execute all the deferred deletions now.
  Status ExecuteDelete();

  void Enable(bool enable) { enabled_ = enable; }

 private:
  bool enabled_;
  std::function<Status(std::string, std::int64_t)> delete_fun_;
  std::vector<std::pair<std::string, std::int64_t>> object_list_;
};

}  // namespace internal

/**
 * Compose existing objects into a new object in the same bucket.
 *
 * Contrary to `Client::ComposeObject`, this function doesn't have a limit on
 * the number of source objects.
 *
 * The implementation may need to perform multiple Client::ComposeObject calls
 * to create intermediate, temporary objects which are then further composed.
 * Due to the lack of atomicity of this series of operations, stray temporary
 * objects might be left over if there are transient failures. In order to allow
 * the user to easily control for such situations, the user is expected to
 * provide a unique @p prefix parameter, which will become the prefix of all the
 * temporary objects created by this function. Once this function finishes, the
 * user may safely remove all objects with the provided prefix (e.g. via
 * DeleteByPrefix()). We recommend using CreateRandomPrefixName() for selecting
 * a random prefix within a bucket.
 *
 * @param client the client on which to perform the operations needed by this
 *     function
 * @param bucket_name the name of the bucket used for source object and
 *     destination object.
 * @param source_objects objects used to compose `destination_object_name`.
 * @param destination_object_name the composed object name.
 * @param prefix prefix for temporary objects created by this function; there
 *     should not be any objects with this prefix; in order to avoid race
 *     conditions, this function will create an object with this name
 * @param ignore_cleanup_failures if the composition succeeds but cleanup of
 *     temporary objects fails, depending on this parameter either a success
 *     will be returned (`true`) or the relevant cleanup error (`false`)
 * @param options a list of optional query parameters and/or request headers.
 *     Valid types for this operation include `DestinationPredefinedAcl`,
 *     `EncryptionKey`, `IfGenerationMatch`, `IfMetagenerationMatch`
 *     `KmsKeyName`, `QuotaUser`, `UserIp`, `UserProject` and
 *     `WithObjectMetadata`.
 *
 * @par Idempotency
 * This operation is not idempotent. While each request performed by this
 * function is retried based on the client policies, the operation itself stops
 * on the first request that fails.
 *
 * @par Example
 * @snippet storage_object_samples.cc compose object from many
 */
template <typename... Options>
StatusOr<ObjectMetadata> ComposeMany(
    Client& client, std::string const& bucket_name,
    std::vector<ComposeSourceObject> source_objects, std::string const& prefix,
    std::string destination_object_name, bool ignore_cleanup_failures,
    Options&&... options) {
  using internal::Among;
  using internal::NotAmong;
  using internal::StaticTupleFilter;
  std::size_t const max_num_objects = 32;

  if (source_objects.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "ComposeMany requires at least one source object.");
  }

  auto all_options = std::make_tuple(options...);

  // TODO(#3247): this list of type should somehow be generated
  static_assert(
      std::tuple_size<decltype(
              StaticTupleFilter<NotAmong<
                  DestinationPredefinedAcl, EncryptionKey, IfGenerationMatch,
                  IfMetagenerationMatch, KmsKeyName, QuotaUser, UserIp,
                  UserProject, WithObjectMetadata>::TPred>(
                  all_options))>::value == 0,
      "This functions accepts only options of type DestinationPredefinedAcl, "
      "EncryptionKey, IfGenerationMatch, IfMetagenerationMatch, KmsKeyName, "
      "QuotaUser, UserIp, UserProject or WithObjectMetadata.");

  internal::ScopedDeleter deleter(
      [&](std::string const& object_name, std::int64_t generation) {
        return google::cloud::internal::apply(
            internal::DeleteApplyHelper{client, bucket_name, object_name},
            std::tuple_cat(
                std::make_tuple(IfGenerationMatch(generation)),
                StaticTupleFilter<Among<QuotaUser, UserProject, UserIp>::TPred>(
                    all_options)));
      });

  auto lock = internal::LockPrefix(client, bucket_name, prefix, "",
                                   std::make_tuple(options...));
  if (!lock) {
    return Status(
        lock.status().code(),
        "Failed to lock prefix for ComposeMany: " + lock.status().message());
  }
  deleter.Add(*lock);

  std::size_t num_tmp_objects = 0;
  auto tmpobject_name_gen = [&num_tmp_objects, &prefix] {
    return prefix + ".compose-tmp-" + std::to_string(num_tmp_objects++);
  };

  auto to_source_objects = [](std::vector<ObjectMetadata> objects) {
    std::vector<ComposeSourceObject> sources(objects.size());
    std::transform(objects.begin(), objects.end(), sources.begin(),
                   [](ObjectMetadata const& m) {
                     return ComposeSourceObject{m.name(), m.generation(), {}};
                   });
    return sources;
  };

  auto composer = [&](std::vector<ComposeSourceObject> compose_range,
                      bool is_final) -> StatusOr<ObjectMetadata> {
    if (is_final) {
      return google::cloud::internal::apply(
          internal::ComposeApplyHelper{client, bucket_name,
                                       std::move(compose_range),
                                       std::move(destination_object_name)},
          std::tuple_cat(std::make_tuple(IfGenerationMatch(0)), all_options));
    }
    return google::cloud::internal::apply(
        internal::ComposeApplyHelper{client, bucket_name,
                                     std::move(compose_range),
                                     tmpobject_name_gen()},
        StaticTupleFilter<
            NotAmong<IfGenerationMatch, IfMetagenerationMatch>::TPred>(
            all_options));
  };

  auto reduce = [&](std::vector<ComposeSourceObject> source_objects)
      -> StatusOr<std::vector<ObjectMetadata>> {
    std::vector<ObjectMetadata> objects;
    for (auto range_begin = source_objects.begin();
         range_begin != source_objects.end();) {
      std::size_t range_size = std::min<std::size_t>(
          std::distance(range_begin, source_objects.end()), max_num_objects);
      auto range_end = std::next(range_begin, range_size);
      std::vector<ComposeSourceObject> compose_range(range_size);
      std::move(range_begin, range_end, compose_range.begin());

      bool const is_final_composition =
          source_objects.size() <= max_num_objects;
      auto object = composer(std::move(compose_range), is_final_composition);
      if (!object) {
        return std::move(object).status();
      }
      objects.push_back(*std::move(object));
      if (!is_final_composition) {
        deleter.Add(objects.back());
      }
      range_begin = range_end;
    }
    return objects;
  };

  StatusOr<ObjectMetadata> result;
  do {
    StatusOr<std::vector<ObjectMetadata>> objects = reduce(source_objects);
    if (!objects) {
      return objects.status();
    }
    if (objects->size() == 1) {
      if (!ignore_cleanup_failures) {
        auto delete_status = deleter.ExecuteDelete();
        if (!delete_status.ok()) {
          return delete_status;
        }
      }
      result = std::move((*objects)[0]);
      break;
    }
    source_objects = to_source_objects(*std::move(objects));
  } while (source_objects.size() > 1);
  return result;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_CLIENT_H
