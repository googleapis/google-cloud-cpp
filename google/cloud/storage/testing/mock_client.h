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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockClient : public google::cloud::storage::internal::RawClient {
 public:
  MOCK_CONST_METHOD0(client_options, ClientOptions const&());
  MOCK_METHOD1(ListBuckets, StatusOr<internal::ListBucketsResponse>(
                                internal::ListBucketsRequest const&));
  MOCK_METHOD1(CreateBucket, StatusOr<storage::BucketMetadata>(
                                 internal::CreateBucketRequest const&));
  MOCK_METHOD1(GetBucketMetadata,
               StatusOr<storage::BucketMetadata>(
                   internal::GetBucketMetadataRequest const&));
  MOCK_METHOD1(DeleteBucket, StatusOr<internal::EmptyResponse>(
                                 internal::DeleteBucketRequest const&));
  MOCK_METHOD1(UpdateBucket, StatusOr<storage::BucketMetadata>(
                                 internal::UpdateBucketRequest const&));
  MOCK_METHOD1(PatchBucket, StatusOr<storage::BucketMetadata>(
                                internal::PatchBucketRequest const&));
  MOCK_METHOD1(GetBucketIamPolicy,
               StatusOr<IamPolicy>(internal::GetBucketIamPolicyRequest const&));
  MOCK_METHOD1(
      GetNativeBucketIamPolicy,
      StatusOr<NativeIamPolicy>(internal::GetBucketIamPolicyRequest const&));
  MOCK_METHOD1(SetBucketIamPolicy,
               StatusOr<IamPolicy>(internal::SetBucketIamPolicyRequest const&));
  MOCK_METHOD1(SetNativeBucketIamPolicy,
               StatusOr<NativeIamPolicy>(
                   internal::SetNativeBucketIamPolicyRequest const&));
  MOCK_METHOD1(TestBucketIamPermissions,
               StatusOr<internal::TestBucketIamPermissionsResponse>(
                   internal::TestBucketIamPermissionsRequest const&));
  MOCK_METHOD1(LockBucketRetentionPolicy,
               StatusOr<BucketMetadata>(
                   internal::LockBucketRetentionPolicyRequest const&));

  MOCK_METHOD1(InsertObjectMedia,
               StatusOr<storage::ObjectMetadata>(
                   internal::InsertObjectMediaRequest const&));
  MOCK_METHOD1(CopyObject, StatusOr<storage::ObjectMetadata>(
                               internal::CopyObjectRequest const&));
  MOCK_METHOD1(GetObjectMetadata,
               StatusOr<storage::ObjectMetadata>(
                   internal::GetObjectMetadataRequest const&));
  MOCK_METHOD1(ReadObject,
               StatusOr<std::unique_ptr<internal::ObjectReadSource>>(
                   internal::ReadObjectRangeRequest const&));
  MOCK_METHOD1(ListObjects, StatusOr<internal::ListObjectsResponse>(
                                internal::ListObjectsRequest const&));
  MOCK_METHOD1(DeleteObject, StatusOr<internal::EmptyResponse>(
                                 internal::DeleteObjectRequest const&));
  MOCK_METHOD1(UpdateObject, StatusOr<storage::ObjectMetadata>(
                                 internal::UpdateObjectRequest const&));
  MOCK_METHOD1(PatchObject, StatusOr<storage::ObjectMetadata>(
                                internal::PatchObjectRequest const&));
  MOCK_METHOD1(ComposeObject, StatusOr<storage::ObjectMetadata>(
                                  internal::ComposeObjectRequest const&));
  MOCK_METHOD1(RewriteObject, StatusOr<internal::RewriteObjectResponse>(
                                  internal::RewriteObjectRequest const&));
  MOCK_METHOD1(CreateResumableSession,
               StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
                   internal::ResumableUploadRequest const&));
  MOCK_METHOD1(RestoreResumableSession,
               StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
                   std::string const&));

  MOCK_METHOD1(ListBucketAcl, StatusOr<internal::ListBucketAclResponse>(
                                  internal::ListBucketAclRequest const&));
  MOCK_METHOD1(CreateBucketAcl, StatusOr<BucketAccessControl>(
                                    internal::CreateBucketAclRequest const&));
  MOCK_METHOD1(DeleteBucketAcl, StatusOr<internal::EmptyResponse>(
                                    internal::DeleteBucketAclRequest const&));
  MOCK_METHOD1(GetBucketAcl, StatusOr<BucketAccessControl>(
                                 internal::GetBucketAclRequest const&));
  MOCK_METHOD1(UpdateBucketAcl, StatusOr<BucketAccessControl>(
                                    internal::UpdateBucketAclRequest const&));
  MOCK_METHOD1(PatchBucketAcl, StatusOr<BucketAccessControl>(
                                   internal::PatchBucketAclRequest const&));

  MOCK_METHOD1(ListObjectAcl, StatusOr<internal::ListObjectAclResponse>(
                                  internal::ListObjectAclRequest const&));
  MOCK_METHOD1(CreateObjectAcl, StatusOr<ObjectAccessControl>(
                                    internal::CreateObjectAclRequest const&));
  MOCK_METHOD1(DeleteObjectAcl, StatusOr<internal::EmptyResponse>(
                                    internal::DeleteObjectAclRequest const&));
  MOCK_METHOD1(GetObjectAcl, StatusOr<ObjectAccessControl>(
                                 internal::GetObjectAclRequest const&));
  MOCK_METHOD1(UpdateObjectAcl, StatusOr<ObjectAccessControl>(
                                    internal::UpdateObjectAclRequest const&));
  MOCK_METHOD1(PatchObjectAcl, StatusOr<ObjectAccessControl>(
                                   internal::PatchObjectAclRequest const&));

  MOCK_METHOD1(ListDefaultObjectAcl,
               StatusOr<internal::ListDefaultObjectAclResponse>(
                   internal::ListDefaultObjectAclRequest const&));
  MOCK_METHOD1(CreateDefaultObjectAcl,
               StatusOr<ObjectAccessControl>(
                   internal::CreateDefaultObjectAclRequest const&));
  MOCK_METHOD1(DeleteDefaultObjectAcl,
               StatusOr<internal::EmptyResponse>(
                   internal::DeleteDefaultObjectAclRequest const&));
  MOCK_METHOD1(GetDefaultObjectAcl,
               StatusOr<ObjectAccessControl>(
                   internal::GetDefaultObjectAclRequest const&));
  MOCK_METHOD1(UpdateDefaultObjectAcl,
               StatusOr<ObjectAccessControl>(
                   internal::UpdateDefaultObjectAclRequest const&));
  MOCK_METHOD1(PatchDefaultObjectAcl,
               StatusOr<ObjectAccessControl>(
                   internal::PatchDefaultObjectAclRequest const&));

  MOCK_METHOD1(GetServiceAccount,
               StatusOr<ServiceAccount>(
                   internal::GetProjectServiceAccountRequest const&));
  MOCK_METHOD1(ListHmacKeys, StatusOr<internal::ListHmacKeysResponse>(
                                 internal::ListHmacKeysRequest const&));
  MOCK_METHOD1(CreateHmacKey, StatusOr<internal::CreateHmacKeyResponse>(
                                  internal::CreateHmacKeyRequest const&));
  MOCK_METHOD1(DeleteHmacKey, StatusOr<internal::EmptyResponse>(
                                  internal::DeleteHmacKeyRequest const&));
  MOCK_METHOD1(GetHmacKey,
               StatusOr<HmacKeyMetadata>(internal::GetHmacKeyRequest const&));
  MOCK_METHOD1(UpdateHmacKey, StatusOr<HmacKeyMetadata>(
                                  internal::UpdateHmacKeyRequest const&));
  MOCK_METHOD1(SignBlob, StatusOr<internal::SignBlobResponse>(
                             internal::SignBlobRequest const&));

  MOCK_METHOD1(ListNotifications,
               StatusOr<internal::ListNotificationsResponse>(
                   internal::ListNotificationsRequest const&));
  MOCK_METHOD1(CreateNotification,
               StatusOr<NotificationMetadata>(
                   internal::CreateNotificationRequest const&));
  MOCK_METHOD1(GetNotification, StatusOr<NotificationMetadata>(
                                    internal::GetNotificationRequest const&));
  MOCK_METHOD1(DeleteNotification,
               StatusOr<internal::EmptyResponse>(
                   internal::DeleteNotificationRequest const&));
  MOCK_METHOD1(
      AuthorizationHeader,
      StatusOr<std::string>(
          std::shared_ptr<google::cloud::storage::oauth2::Credentials> const&));
};

class MockResumableUploadSession
    : public google::cloud::storage::internal::ResumableUploadSession {
 public:
  MOCK_METHOD1(UploadChunk, StatusOr<internal::ResumableUploadResponse>(
                                std::string const& buffer));
  MOCK_METHOD2(UploadFinalChunk,
               StatusOr<internal::ResumableUploadResponse>(
                   std::string const& buffer, std::uint64_t upload_size));
  MOCK_METHOD0(ResetSession, StatusOr<internal::ResumableUploadResponse>());
  MOCK_CONST_METHOD0(next_expected_byte, std::uint64_t());
  MOCK_CONST_METHOD0(session_id, std::string const&());
  MOCK_CONST_METHOD0(done, bool());
  MOCK_CONST_METHOD0(last_response,
                     StatusOr<internal::ResumableUploadResponse> const&());
};

class MockObjectReadSource : public internal::ObjectReadSource {
 public:
  MOCK_CONST_METHOD0(IsOpen, bool());
  MOCK_METHOD0(Close, StatusOr<internal::HttpResponse>());
  MOCK_METHOD2(Read,
               StatusOr<internal::ReadSourceResult>(char* buf, std::size_t n));
};

class MockStreambuf : public internal::ObjectWriteStreambuf {
 public:
  MOCK_CONST_METHOD0(IsOpen, bool());
  MOCK_METHOD0(DoClose, StatusOr<internal::HttpResponse>());
  MOCK_METHOD1(ValidateHash, bool(ObjectMetadata const&));
  MOCK_CONST_METHOD0(received_hash, std::string const&());
  MOCK_CONST_METHOD0(computed_hash, std::string const&());
  MOCK_CONST_METHOD0(resumable_session_id, std::string const&());
  MOCK_CONST_METHOD0(next_expected_byte, std::uint64_t());
};
}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_
