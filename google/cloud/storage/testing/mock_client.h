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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockClient : public google::cloud::storage::internal::RawClient {
 public:
  MOCK_METHOD(ClientOptions const&, client_options, (), (const, override));
  MOCK_METHOD(StatusOr<internal::ListBucketsResponse>, ListBuckets,
              (internal::ListBucketsRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, CreateBucket,
              (internal::CreateBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, GetBucketMetadata,
              (internal::GetBucketMetadataRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteBucket,
              (internal::DeleteBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, UpdateBucket,
              (internal::UpdateBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, PatchBucket,
              (internal::PatchBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<IamPolicy>, GetBucketIamPolicy,
              (internal::GetBucketIamPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<NativeIamPolicy>, GetNativeBucketIamPolicy,
              (internal::GetBucketIamPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<IamPolicy>, SetBucketIamPolicy,
              (internal::SetBucketIamPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<NativeIamPolicy>, SetNativeBucketIamPolicy,
              (internal::SetNativeBucketIamPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::TestBucketIamPermissionsResponse>,
              TestBucketIamPermissions,
              (internal::TestBucketIamPermissionsRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketMetadata>, LockBucketRetentionPolicy,
              (internal::LockBucketRetentionPolicyRequest const&), (override));

  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, InsertObjectMedia,
              (internal::InsertObjectMediaRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, CopyObject,
              (internal::CopyObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, GetObjectMetadata,
              (internal::GetObjectMetadataRequest const&), (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<internal::ObjectReadSource>>, ReadObject,
              (internal::ReadObjectRangeRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::ListObjectsResponse>, ListObjects,
              (internal::ListObjectsRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteObject,
              (internal::DeleteObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, UpdateObject,
              (internal::UpdateObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, PatchObject,
              (internal::PatchObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, ComposeObject,
              (internal::ComposeObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::RewriteObjectResponse>, RewriteObject,
              (internal::RewriteObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<internal::ResumableUploadSession>>,
              CreateResumableSession, (internal::ResumableUploadRequest const&),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<internal::ResumableUploadSession>>,
              RestoreResumableSession, (std::string const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteResumableUpload,
              (internal::DeleteResumableUploadRequest const&), (override));

  MOCK_METHOD(StatusOr<internal::ListBucketAclResponse>, ListBucketAcl,
              (internal::ListBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, CreateBucketAcl,
              (internal::CreateBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteBucketAcl,
              (internal::DeleteBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, GetBucketAcl,
              (internal::GetBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, UpdateBucketAcl,
              (internal::UpdateBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, PatchBucketAcl,
              (internal::PatchBucketAclRequest const&), (override));

  MOCK_METHOD(StatusOr<internal::ListObjectAclResponse>, ListObjectAcl,
              (internal::ListObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, CreateObjectAcl,
              (internal::CreateObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteObjectAcl,
              (internal::DeleteObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, GetObjectAcl,
              (internal::GetObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, UpdateObjectAcl,
              (internal::UpdateObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, PatchObjectAcl,
              (internal::PatchObjectAclRequest const&), (override));

  MOCK_METHOD(StatusOr<internal::ListDefaultObjectAclResponse>,
              ListDefaultObjectAcl,
              (internal::ListDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, CreateDefaultObjectAcl,
              (internal::CreateDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteDefaultObjectAcl,
              (internal::DeleteDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, GetDefaultObjectAcl,
              (internal::GetDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, UpdateDefaultObjectAcl,
              (internal::UpdateDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, PatchDefaultObjectAcl,
              (internal::PatchDefaultObjectAclRequest const&), (override));

  MOCK_METHOD(StatusOr<ServiceAccount>, GetServiceAccount,
              (internal::GetProjectServiceAccountRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::ListHmacKeysResponse>, ListHmacKeys,
              (internal::ListHmacKeysRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::CreateHmacKeyResponse>, CreateHmacKey,
              (internal::CreateHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteHmacKey,
              (internal::DeleteHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<HmacKeyMetadata>, GetHmacKey,
              (internal::GetHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<HmacKeyMetadata>, UpdateHmacKey,
              (internal::UpdateHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::SignBlobResponse>, SignBlob,
              (internal::SignBlobRequest const&), (override));

  MOCK_METHOD(StatusOr<internal::ListNotificationsResponse>, ListNotifications,
              (internal::ListNotificationsRequest const&), (override));
  MOCK_METHOD(StatusOr<NotificationMetadata>, CreateNotification,
              (internal::CreateNotificationRequest const&), (override));
  MOCK_METHOD(StatusOr<NotificationMetadata>, GetNotification,
              (internal::GetNotificationRequest const&), (override));
  MOCK_METHOD(StatusOr<internal::EmptyResponse>, DeleteNotification,
              (internal::DeleteNotificationRequest const&), (override));
  MOCK_METHOD(
      StatusOr<std::string>, AuthorizationHeader,
      (std::shared_ptr<google::cloud::storage::oauth2::Credentials> const&));
};

class MockResumableUploadSession
    : public google::cloud::storage::internal::ResumableUploadSession {
 public:
  MOCK_METHOD(StatusOr<internal::ResumableUploadResponse>, UploadChunk,
              (internal::ConstBufferSequence const& buffer), (override));
  MOCK_METHOD(StatusOr<internal::ResumableUploadResponse>, UploadFinalChunk,
              (internal::ConstBufferSequence const& buffer,
               std::uint64_t upload_size),
              (override));
  MOCK_METHOD(StatusOr<internal::ResumableUploadResponse>, ResetSession, (),
              (override));
  MOCK_METHOD(std::uint64_t, next_expected_byte, (), (const, override));
  MOCK_METHOD(std::string const&, session_id, (), (const, override));
  MOCK_METHOD(bool, done, (), (const, override));
  MOCK_METHOD(StatusOr<internal::ResumableUploadResponse> const&, last_response,
              (), (const, override));
};

class MockObjectReadSource : public internal::ObjectReadSource {
 public:
  MOCK_METHOD(bool, IsOpen, (), (const, override));
  MOCK_METHOD(StatusOr<internal::HttpResponse>, Close, (), (override));
  MOCK_METHOD(StatusOr<internal::ReadSourceResult>, Read,
              (char* buf, std::size_t n), (override));
};

class MockStreambuf : public internal::ObjectWriteStreambuf {
 public:
  MOCK_METHOD(bool, IsOpen, (), (const, override));
  MOCK_METHOD(StatusOr<internal::HttpResponse>, DoClose, ());
  MOCK_METHOD(bool, ValidateHash, (ObjectMetadata const&), (override));
  MOCK_METHOD(std::string const&, received_hash, (), (const, override));
  MOCK_METHOD(std::string const&, computed_hash, (), (const, override));
  MOCK_METHOD(std::string const&, resumable_session_id, (), (const, override));
  MOCK_METHOD(std::uint64_t, next_expected_byte, (), (const, override));
};

/// Create a client configured to use the given mock.
template <typename... Policies>
Client ClientFromMock(std::shared_ptr<MockClient> const& mock,
                      Policies&&... p) {
  return internal::ClientImplDetails::CreateClient(
      mock, std::forward<Policies>(p)...);
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H
