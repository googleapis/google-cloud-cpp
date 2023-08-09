// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H

#include "google/cloud/storage/internal/curl/handle_factory.h"
#include "google/cloud/storage/internal/generic_stub.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/random.h"
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
class CurlRequestBuilder;

/**
 * Computes the `Host: ` header given the options and service.
 *
 * Returns an empty string when the `libcurl` default is appropriate, and the
 * full header in other cases.  The most common case where the default is not
 * correct are applications targeting `private.googleapis.com` or
 * `restricted.googleapis.com`.
 *
 * @see https://cloud.google.com/vpc/docs/configure-private-google-access-hybrid
 * @see
 * https://cloud.google.com/vpc-service-controls/docs/set-up-private-connectivity
 */
std::string HostHeader(Options const& options, char const* service);

/**
 * Implements the low-level RPCs to Google Cloud Storage using libcurl.
 */
class CurlClient : public storage_internal::GenericStub {
 public:
  // The constructor is private because the class must always be created
  // as a shared_ptr<>.
  explicit CurlClient(Options options);

  CurlClient(CurlClient const& rhs) = delete;
  CurlClient(CurlClient&& rhs) = delete;
  CurlClient& operator=(CurlClient const& rhs) = delete;
  CurlClient& operator=(CurlClient&& rhs) = delete;

  Options options() const override;

  StatusOr<ListBucketsResponse> ListBuckets(
      rest_internal::RestContext& context, Options const& options,
      ListBucketsRequest const& request) override;
  StatusOr<BucketMetadata> CreateBucket(
      rest_internal::RestContext& context, Options const& options,
      CreateBucketRequest const& request) override;
  StatusOr<BucketMetadata> GetBucketMetadata(
      rest_internal::RestContext& context, Options const& options,
      GetBucketMetadataRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucket(
      rest_internal::RestContext& context, Options const& options,
      DeleteBucketRequest const& request) override;
  StatusOr<BucketMetadata> UpdateBucket(
      rest_internal::RestContext& context, Options const& options,
      UpdateBucketRequest const& request) override;
  StatusOr<BucketMetadata> PatchBucket(
      rest_internal::RestContext& context, Options const& options,
      PatchBucketRequest const& request) override;
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      rest_internal::RestContext& context, Options const& options,
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      rest_internal::RestContext& context, Options const& options,
      SetNativeBucketIamPolicyRequest const& request) override;
  StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      rest_internal::RestContext& context, Options const& options,
      TestBucketIamPermissionsRequest const& request) override;
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      rest_internal::RestContext& context, Options const& options,
      LockBucketRetentionPolicyRequest const& request) override;

  StatusOr<ObjectMetadata> InsertObjectMedia(
      rest_internal::RestContext& context, Options const& options,
      InsertObjectMediaRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      rest_internal::RestContext& context, Options const& options,
      GetObjectMetadataRequest const& request) override;
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      rest_internal::RestContext& context, Options const& options,
      ReadObjectRangeRequest const& request) override;
  StatusOr<ListObjectsResponse> ListObjects(
      rest_internal::RestContext& context, Options const& options,
      ListObjectsRequest const& request) override;
  StatusOr<EmptyResponse> DeleteObject(
      rest_internal::RestContext& context, Options const& options,
      DeleteObjectRequest const& request) override;
  StatusOr<ObjectMetadata> UpdateObject(
      rest_internal::RestContext& context, Options const& options,
      UpdateObjectRequest const& request) override;
  StatusOr<ObjectMetadata> PatchObject(
      rest_internal::RestContext& context, Options const& options,
      PatchObjectRequest const& request) override;
  StatusOr<ObjectMetadata> ComposeObject(
      rest_internal::RestContext& context, Options const& options,
      ComposeObjectRequest const& request) override;

  StatusOr<CreateResumableUploadResponse> CreateResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      ResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> QueryResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      QueryResumableUploadRequest const& request) override;
  StatusOr<EmptyResponse> DeleteResumableUpload(
      rest_internal::RestContext& context, Options const& options,
      DeleteResumableUploadRequest const& request) override;
  StatusOr<QueryResumableUploadResponse> UploadChunk(
      rest_internal::RestContext& context, Options const& options,
      UploadChunkRequest const& request) override;

  StatusOr<ListBucketAclResponse> ListBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      ListBucketAclRequest const& request) override;
  StatusOr<ObjectMetadata> CopyObject(
      rest_internal::RestContext& context, Options const& options,
      CopyObjectRequest const& request) override;
  StatusOr<BucketAccessControl> CreateBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      CreateBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> GetBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      GetBucketAclRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      DeleteBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> UpdateBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      UpdateBucketAclRequest const& request) override;
  StatusOr<BucketAccessControl> PatchBucketAcl(
      rest_internal::RestContext& context, Options const& options,
      PatchBucketAclRequest const& request) override;

  StatusOr<ListObjectAclResponse> ListObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      ListObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      CreateObjectAclRequest const& request) override;
  StatusOr<EmptyResponse> DeleteObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      DeleteObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> GetObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      GetObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> UpdateObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      UpdateObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      PatchObjectAclRequest const& request) override;
  StatusOr<RewriteObjectResponse> RewriteObject(
      rest_internal::RestContext& context, Options const& options,
      RewriteObjectRequest const& request) override;

  StatusOr<ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      ListDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      CreateDefaultObjectAclRequest const& request) override;
  StatusOr<EmptyResponse> DeleteDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      DeleteDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      GetDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      UpdateDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      rest_internal::RestContext& context, Options const& options,
      PatchDefaultObjectAclRequest const& request) override;

  StatusOr<ServiceAccount> GetServiceAccount(
      rest_internal::RestContext& context, Options const& options,
      GetProjectServiceAccountRequest const& request) override;
  StatusOr<ListHmacKeysResponse> ListHmacKeys(
      rest_internal::RestContext& context, Options const& options,
      ListHmacKeysRequest const& request) override;
  StatusOr<CreateHmacKeyResponse> CreateHmacKey(
      rest_internal::RestContext& context, Options const& options,
      CreateHmacKeyRequest const& request) override;
  StatusOr<EmptyResponse> DeleteHmacKey(
      rest_internal::RestContext& context, Options const& options,
      DeleteHmacKeyRequest const& request) override;
  StatusOr<HmacKeyMetadata> GetHmacKey(
      rest_internal::RestContext& context, Options const& options,
      GetHmacKeyRequest const& request) override;
  StatusOr<HmacKeyMetadata> UpdateHmacKey(
      rest_internal::RestContext& context, Options const& options,
      UpdateHmacKeyRequest const& request) override;
  StatusOr<SignBlobResponse> SignBlob(rest_internal::RestContext& context,
                                      Options const& options,
                                      SignBlobRequest const& request) override;

  StatusOr<ListNotificationsResponse> ListNotifications(
      rest_internal::RestContext& context, Options const& options,
      ListNotificationsRequest const& request) override;
  StatusOr<NotificationMetadata> CreateNotification(
      rest_internal::RestContext& context, Options const& options,
      CreateNotificationRequest const& request) override;
  StatusOr<NotificationMetadata> GetNotification(
      rest_internal::RestContext& context, Options const& options,
      GetNotificationRequest const& request) override;
  StatusOr<EmptyResponse> DeleteNotification(
      rest_internal::RestContext& context, Options const& options,
      DeleteNotificationRequest const& request) override;

  std::vector<std::string> InspectStackStructure() const override;

 protected:
 private:
  /// Setup the configuration parameters that do not depend on the request.
  Status SetupBuilderCommon(CurlRequestBuilder& builder,
                            rest_internal::RestContext const& context,
                            Options const& options, char const* method,
                            char const* service = "storage");

  /// Applies the common configuration parameters to @p builder.
  template <typename Request>
  Status SetupBuilder(CurlRequestBuilder& builder,
                      rest_internal::RestContext const& context,
                      Options const& options, Request const& request,
                      char const* method);

  /// Insert an object using uploadType=multipart.
  StatusOr<ObjectMetadata> InsertObjectMediaMultipart(
      rest_internal::RestContext& context, Options const& options,
      InsertObjectMediaRequest const& request);
  std::string MakeBoundary();

  /// Insert an object using uploadType=media.
  StatusOr<ObjectMetadata> InsertObjectMediaSimple(
      rest_internal::RestContext& context, Options const& options,
      InsertObjectMediaRequest const& request);

  google::cloud::Options opts_;
  std::string const x_goog_api_client_header_;
  std::string const storage_endpoint_;
  std::string const upload_endpoint_;
  std::string const xml_endpoint_;
  std::string const iam_endpoint_;

  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;  // GUARDED_BY(mu_);

  std::shared_ptr<rest_internal::CurlHandleFactory> storage_factory_;
  std::shared_ptr<rest_internal::CurlHandleFactory> upload_factory_;
  std::shared_ptr<rest_internal::CurlHandleFactory> xml_upload_factory_;
  std::shared_ptr<rest_internal::CurlHandleFactory> xml_download_factory_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H
