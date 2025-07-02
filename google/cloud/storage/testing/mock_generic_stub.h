// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_GENERIC_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_GENERIC_STUB_H

#include "google/cloud/storage/internal/generic_stub.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

/**
 * Implements a Googlemock for the (generated) GCS+gRPC Stub.
 *
 * @warning This class is intended for the `google-cloud-cpp` tests. It is not
 * part of the public API and subject to change without notice.
 */
class MockGenericStub : public storage_internal::GenericStub {
 public:
  MOCK_METHOD(google::cloud::Options, options, (), (const, override));

  MOCK_METHOD(StatusOr<storage::internal::ListBucketsResponse>, ListBuckets,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListBucketsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, CreateBucket,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CreateBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, GetBucketMetadata,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetBucketMetadataRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteBucket,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, UpdateBucket,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UpdateBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, PatchBucket,
              (rest_internal::RestContext&, Options const&,
               storage::internal::PatchBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::NativeIamPolicy>, GetNativeBucketIamPolicy,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetBucketIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::NativeIamPolicy>, SetNativeBucketIamPolicy,
              (rest_internal::RestContext&, Options const&,
               storage::internal::SetNativeBucketIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::TestBucketIamPermissionsResponse>,
              TestBucketIamPermissions,
              (rest_internal::RestContext&, Options const&,
               storage::internal::TestBucketIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketMetadata>, LockBucketRetentionPolicy,
              (rest_internal::RestContext&, Options const&,
               storage::internal::LockBucketRetentionPolicyRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, InsertObjectMedia,
              (rest_internal::RestContext&, Options const&,
               storage::internal::InsertObjectMediaRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, CopyObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CopyObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, GetObjectMetadata,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetObjectMetadataRequest const&),
              (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>>,
              ReadObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ReadObjectRangeRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::ListObjectsResponse>, ListObjects,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListObjectsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, UpdateObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UpdateObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, MoveObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::MoveObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, PatchObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::PatchObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, ComposeObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ComposeObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::RewriteObjectResponse>, RewriteObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::RewriteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectMetadata>, RestoreObject,
              (rest_internal::RestContext&, Options const&,
               storage::internal::RestoreObjectRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::internal::CreateResumableUploadResponse>,
              CreateResumableUpload,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ResumableUploadRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::QueryResumableUploadResponse>,
              QueryResumableUpload,
              (rest_internal::RestContext&, Options const&,
               storage::internal::QueryResumableUploadRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteResumableUpload,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteResumableUploadRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::QueryResumableUploadResponse>,
              UploadChunk,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UploadChunkRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::internal::ListBucketAclResponse>, ListBucketAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListBucketAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketAccessControl>, CreateBucketAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CreateBucketAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteBucketAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteBucketAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketAccessControl>, GetBucketAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetBucketAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketAccessControl>, UpdateBucketAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UpdateBucketAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::BucketAccessControl>, PatchBucketAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::PatchBucketAclRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::internal::ListObjectAclResponse>, ListObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, CreateObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CreateObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, GetObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, UpdateObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UpdateObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, PatchObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::PatchObjectAclRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::internal::ListDefaultObjectAclResponse>,
              ListDefaultObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListDefaultObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, CreateDefaultObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CreateDefaultObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>,
              DeleteDefaultObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteDefaultObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, GetDefaultObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetDefaultObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, UpdateDefaultObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UpdateDefaultObjectAclRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::ObjectAccessControl>, PatchDefaultObjectAcl,
              (rest_internal::RestContext&, Options const&,
               storage::internal::PatchDefaultObjectAclRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::ServiceAccount>, GetServiceAccount,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetProjectServiceAccountRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::ListHmacKeysResponse>, ListHmacKeys,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListHmacKeysRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::CreateHmacKeyResponse>, CreateHmacKey,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CreateHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteHmacKey,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::HmacKeyMetadata>, GetHmacKey,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::HmacKeyMetadata>, UpdateHmacKey,
              (rest_internal::RestContext&, Options const&,
               storage::internal::UpdateHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::SignBlobResponse>, SignBlob,
              (rest_internal::RestContext&, Options const&,
               storage::internal::SignBlobRequest const&),
              (override));

  MOCK_METHOD(StatusOr<storage::internal::ListNotificationsResponse>,
              ListNotifications,
              (rest_internal::RestContext&, Options const&,
               storage::internal::ListNotificationsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::NotificationMetadata>, CreateNotification,
              (rest_internal::RestContext&, Options const&,
               storage::internal::CreateNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::NotificationMetadata>, GetNotification,
              (rest_internal::RestContext&, Options const&,
               storage::internal::GetNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<storage::internal::EmptyResponse>, DeleteNotification,
              (rest_internal::RestContext&, Options const&,
               storage::internal::DeleteNotificationRequest const&),
              (override));

  MOCK_METHOD(std::vector<std::string>, InspectStackStructure, (),
              (const, override));
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_GENERIC_STUB_H
