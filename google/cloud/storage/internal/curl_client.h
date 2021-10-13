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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H

#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
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
class CurlClient : public RawClient,
                   public std::enable_shared_from_this<CurlClient> {
 public:
  static std::shared_ptr<CurlClient> Create(Options options) {
    // Cannot use std::make_shared because the constructor is private.
    return std::shared_ptr<CurlClient>(new CurlClient(std::move(options)));
  }
  static std::shared_ptr<CurlClient> Create(ClientOptions options) {
    return Create(MakeOptions(std::move(options)));
  }

  CurlClient(CurlClient const& rhs) = delete;
  CurlClient(CurlClient&& rhs) = delete;
  CurlClient& operator=(CurlClient const& rhs) = delete;
  CurlClient& operator=(CurlClient&& rhs) = delete;

  using LockFunction =
      std::function<void(CURL*, curl_lock_data, curl_lock_access)>;
  using UnlockFunction = std::function<void(CURL*, curl_lock_data)>;

  //@{
  /// @name Implement the CurlResumableSession operations.
  // Note that these member functions are not inherited from RawClient, they are
  // called only by `CurlResumableUploadSession`, because the retry loop for
  // them is very different from the standard retry loop. Also note that some of
  // these member functions are virtual, but only because we need to override
  // them in the *library* unit tests.
  virtual StatusOr<ResumableUploadResponse> UploadChunk(
      UploadChunkRequest const&);
  virtual StatusOr<ResumableUploadResponse> QueryResumableUpload(
      QueryResumableUploadRequest const&);
  StatusOr<std::unique_ptr<ResumableUploadSession>>
  FullyRestoreResumableSession(ResumableUploadRequest const& request,
                               std::string const& session_id);
  //@}

  ClientOptions const& client_options() const override {
    return backwards_compatibility_options_;
  }

  StatusOr<ListBucketsResponse> ListBuckets(
      ListBucketsRequest const& request) override;
  StatusOr<BucketMetadata> CreateBucket(
      CreateBucketRequest const& request) override;
  StatusOr<BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) override;
  StatusOr<EmptyResponse> DeleteBucket(DeleteBucketRequest const&) override;
  StatusOr<BucketMetadata> UpdateBucket(
      UpdateBucketRequest const& request) override;
  StatusOr<BucketMetadata> PatchBucket(
      PatchBucketRequest const& request) override;
  StatusOr<IamPolicy> GetBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> GetNativeBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) override;
  StatusOr<IamPolicy> SetBucketIamPolicy(
      SetBucketIamPolicyRequest const& request) override;
  StatusOr<NativeIamPolicy> SetNativeBucketIamPolicy(
      SetNativeBucketIamPolicyRequest const& request) override;
  StatusOr<TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      TestBucketIamPermissionsRequest const& request) override;
  StatusOr<BucketMetadata> LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest const& request) override;

  StatusOr<ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const& request) override;
  StatusOr<ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) override;
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObject(
      ReadObjectRangeRequest const&) override;
  StatusOr<ListObjectsResponse> ListObjects(
      ListObjectsRequest const& request) override;
  StatusOr<EmptyResponse> DeleteObject(
      DeleteObjectRequest const& request) override;
  StatusOr<ObjectMetadata> UpdateObject(
      UpdateObjectRequest const& request) override;
  StatusOr<ObjectMetadata> PatchObject(
      PatchObjectRequest const& request) override;
  StatusOr<ObjectMetadata> ComposeObject(
      ComposeObjectRequest const& request) override;
  StatusOr<std::unique_ptr<ResumableUploadSession>> CreateResumableSession(
      ResumableUploadRequest const& request) override;
  StatusOr<EmptyResponse> DeleteResumableUpload(
      DeleteResumableUploadRequest const& request) override;

  StatusOr<ListBucketAclResponse> ListBucketAcl(
      ListBucketAclRequest const& request) override;
  StatusOr<ObjectMetadata> CopyObject(
      CopyObjectRequest const& request) override;
  StatusOr<BucketAccessControl> CreateBucketAcl(
      CreateBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> GetBucketAcl(
      GetBucketAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteBucketAcl(
      DeleteBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> UpdateBucketAcl(
      UpdateBucketAclRequest const&) override;
  StatusOr<BucketAccessControl> PatchBucketAcl(
      PatchBucketAclRequest const&) override;

  StatusOr<ListObjectAclResponse> ListObjectAcl(
      ListObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateObjectAcl(
      CreateObjectAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteObjectAcl(
      DeleteObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> GetObjectAcl(
      GetObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> UpdateObjectAcl(
      UpdateObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> PatchObjectAcl(
      PatchObjectAclRequest const&) override;
  StatusOr<RewriteObjectResponse> RewriteObject(
      RewriteObjectRequest const&) override;

  StatusOr<ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      ListDefaultObjectAclRequest const& request) override;
  StatusOr<ObjectAccessControl> CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest const&) override;
  StatusOr<EmptyResponse> DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> GetDefaultObjectAcl(
      GetDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest const&) override;
  StatusOr<ObjectAccessControl> PatchDefaultObjectAcl(
      PatchDefaultObjectAclRequest const&) override;

  StatusOr<ServiceAccount> GetServiceAccount(
      GetProjectServiceAccountRequest const&) override;
  StatusOr<ListHmacKeysResponse> ListHmacKeys(
      ListHmacKeysRequest const&) override;
  StatusOr<CreateHmacKeyResponse> CreateHmacKey(
      CreateHmacKeyRequest const&) override;
  StatusOr<EmptyResponse> DeleteHmacKey(DeleteHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> GetHmacKey(GetHmacKeyRequest const&) override;
  StatusOr<HmacKeyMetadata> UpdateHmacKey(UpdateHmacKeyRequest const&) override;
  StatusOr<SignBlobResponse> SignBlob(SignBlobRequest const&) override;

  StatusOr<ListNotificationsResponse> ListNotifications(
      ListNotificationsRequest const&) override;
  StatusOr<NotificationMetadata> CreateNotification(
      CreateNotificationRequest const&) override;
  StatusOr<NotificationMetadata> GetNotification(
      GetNotificationRequest const&) override;
  StatusOr<EmptyResponse> DeleteNotification(
      DeleteNotificationRequest const&) override;

 protected:
  // The constructor is private because the class must always be created
  // as a shared_ptr<>.
  explicit CurlClient(Options options);

 private:
  /// Setup the configuration parameters that do not depend on the request.
  Status SetupBuilderCommon(CurlRequestBuilder& builder, char const* method,
                            char const* service = "storage");

  /// Applies the common configuration parameters to @p builder.
  template <typename Request>
  Status SetupBuilder(CurlRequestBuilder& builder, Request const& request,
                      char const* method);

  StatusOr<ObjectMetadata> InsertObjectMediaXml(
      InsertObjectMediaRequest const& request);
  StatusOr<std::unique_ptr<ObjectReadSource>> ReadObjectXml(
      ReadObjectRangeRequest const& request);

  /// Insert an object using uploadType=multipart.
  StatusOr<ObjectMetadata> InsertObjectMediaMultipart(
      InsertObjectMediaRequest const& request);
  std::string PickBoundary(std::string const& text_to_avoid);

  /// Insert an object using uploadType=media.
  StatusOr<ObjectMetadata> InsertObjectMediaSimple(
      InsertObjectMediaRequest const& request);

  google::cloud::Options opts_;
  ClientOptions backwards_compatibility_options_;
  std::string const x_goog_api_client_header_;
  std::string const storage_endpoint_;
  std::string const upload_endpoint_;
  std::string const xml_endpoint_;
  std::string const iam_endpoint_;
  bool const xml_enabled_;

  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;  // GUARDED_BY(mu_);

  std::shared_ptr<CurlHandleFactory> storage_factory_;
  std::shared_ptr<CurlHandleFactory> upload_factory_;
  std::shared_ptr<CurlHandleFactory> xml_upload_factory_;
  std::shared_ptr<CurlHandleFactory> xml_download_factory_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H
