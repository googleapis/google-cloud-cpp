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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H_

#include "google/cloud/internal/random.h"
#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/version.h"
#include <mutex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
class CurlRequestBuilder;

/**
 * Implements the low-level RPCs to Google Cloud Storage using libcurl.
 */
class CurlClient : public RawClient,
                   public std::enable_shared_from_this<CurlClient> {
 public:
  static std::shared_ptr<CurlClient> Create(ClientOptions options) {
    // Cannot use std::make_shared because the constructor is private.
    return std::shared_ptr<CurlClient>(new CurlClient(std::move(options)));
  }
  static std::shared_ptr<CurlClient> Create(
      std::shared_ptr<oauth2::Credentials> credentials) {
    return Create(ClientOptions(std::move(credentials)));
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
  // them is very different from the standard retry loop. Also note that these
  // are virtual functions only because we need to override them in the unit
  // tests.
  virtual StatusOr<ResumableUploadResponse> UploadChunk(
      UploadChunkRequest const&);
  virtual StatusOr<ResumableUploadResponse> QueryResumableUpload(
      QueryResumableUploadRequest const&);
  //@}

  ClientOptions const& client_options() const override { return options_; }

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
  StatusOr<std::unique_ptr<ResumableUploadSession>> RestoreResumableSession(
      std::string const& session_id) override;

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

  StatusOr<std::string> AuthorizationHeader(
      std::shared_ptr<google::cloud::storage::oauth2::Credentials> const&);

  void LockShared(curl_lock_data data);
  void UnlockShared(curl_lock_data data);

 protected:
  // The constructor is private because the class must always be created
  // as a shared_ptr<>.
  explicit CurlClient(ClientOptions options);

 private:
  /// Setup the configuration parameters that do not depend on the request.
  Status SetupBuilderCommon(CurlRequestBuilder& builder, char const* method);

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

  template <typename RequestType>
  StatusOr<std::unique_ptr<ResumableUploadSession>>
  CreateResumableSessionGeneric(RequestType const& request);

  ClientOptions options_;
  std::string storage_endpoint_;
  std::string upload_endpoint_;
  std::string xml_upload_endpoint_;
  std::string xml_download_endpoint_;
  std::string iam_endpoint_;

  // These mutexes are used to protect different portions of `share_`.
  std::mutex mu_share_;
  std::mutex mu_dns_;
  std::mutex mu_ssl_session_;
  std::mutex mu_connect_;
  CurlShare share_;

  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;  // GUARDED_BY(mu_);

  // The factories must be listed *after* the CurlShare. libcurl keeps a
  // usage count on each CURLSH* handle, which is only released once the CURL*
  // handle is *closed*. So we want the order of destruction to be (1)
  // factories, as that will delete all the CURL* handles, and then (2) CURLSH*.
  // To guarantee this order just list the members in the opposite order.
  std::shared_ptr<CurlHandleFactory> storage_factory_;
  std::shared_ptr<CurlHandleFactory> upload_factory_;
  std::shared_ptr<CurlHandleFactory> xml_upload_factory_;
  std::shared_ptr<CurlHandleFactory> xml_download_factory_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_CLIENT_H_
