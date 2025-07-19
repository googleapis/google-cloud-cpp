// Copyright 2025 Google LLC
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

#include "google/cloud/storage/internal/storage_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;

// A minimal implementation of StorageConnection to test the default
// implementation of some of its virtual functions.
class TestStorageConnection : public StorageConnection {
 public:
  ~TestStorageConnection() override = default;
  // LCOV_EXCL_START
  MOCK_METHOD(ClientOptions const&, client_options, (), (const, override));
  MOCK_METHOD(StatusOr<ListBucketsResponse>, ListBuckets,
              (ListBucketsRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketMetadata>, CreateBucket,
              (CreateBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketMetadata>, GetBucketMetadata,
              (GetBucketMetadataRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteBucket,
              (DeleteBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketMetadata>, UpdateBucket,
              (UpdateBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketMetadata>, PatchBucket,
              (PatchBucketRequest const&), (override));
  MOCK_METHOD(StatusOr<NativeIamPolicy>, GetNativeBucketIamPolicy,
              (GetBucketIamPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<NativeIamPolicy>, SetNativeBucketIamPolicy,
              (SetNativeBucketIamPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<TestBucketIamPermissionsResponse>,
              TestBucketIamPermissions,
              (TestBucketIamPermissionsRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketMetadata>, LockBucketRetentionPolicy,
              (LockBucketRetentionPolicyRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, InsertObjectMedia,
              (InsertObjectMediaRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, CopyObject, (CopyObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, GetObjectMetadata,
              (GetObjectMetadataRequest const&), (override));
  MOCK_METHOD(StatusOr<std::unique_ptr<ObjectReadSource>>, ReadObject,
              (ReadObjectRangeRequest const&), (override));
  MOCK_METHOD(StatusOr<ListObjectsResponse>, ListObjects,
              (ListObjectsRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteObject,
              (DeleteObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, UpdateObject,
              (UpdateObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, MoveObject, (MoveObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, PatchObject,
              (PatchObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, ComposeObject,
              (ComposeObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<RewriteObjectResponse>, RewriteObject,
              (RewriteObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectMetadata>, RestoreObject,
              (RestoreObjectRequest const&), (override));
  MOCK_METHOD(StatusOr<CreateResumableUploadResponse>, CreateResumableUpload,
              (ResumableUploadRequest const&), (override));
  MOCK_METHOD(StatusOr<QueryResumableUploadResponse>, QueryResumableUpload,
              (QueryResumableUploadRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteResumableUpload,
              (DeleteResumableUploadRequest const&), (override));
  MOCK_METHOD(StatusOr<QueryResumableUploadResponse>, UploadChunk,
              (UploadChunkRequest const&), (override));
  MOCK_METHOD(StatusOr<ListBucketAclResponse>, ListBucketAcl,
              (ListBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, CreateBucketAcl,
              (CreateBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteBucketAcl,
              (DeleteBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, GetBucketAcl,
              (GetBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, UpdateBucketAcl,
              (UpdateBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<BucketAccessControl>, PatchBucketAcl,
              (PatchBucketAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ListObjectAclResponse>, ListObjectAcl,
              (ListObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, CreateObjectAcl,
              (CreateObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteObjectAcl,
              (DeleteObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, GetObjectAcl,
              (GetObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, UpdateObjectAcl,
              (UpdateObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, PatchObjectAcl,
              (PatchObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ListDefaultObjectAclResponse>, ListDefaultObjectAcl,
              (ListDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, CreateDefaultObjectAcl,
              (CreateDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteDefaultObjectAcl,
              (DeleteDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, GetDefaultObjectAcl,
              (GetDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, UpdateDefaultObjectAcl,
              (UpdateDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ObjectAccessControl>, PatchDefaultObjectAcl,
              (PatchDefaultObjectAclRequest const&), (override));
  MOCK_METHOD(StatusOr<ServiceAccount>, GetServiceAccount,
              (GetProjectServiceAccountRequest const&), (override));
  MOCK_METHOD(StatusOr<ListHmacKeysResponse>, ListHmacKeys,
              (ListHmacKeysRequest const&), (override));
  MOCK_METHOD(StatusOr<CreateHmacKeyResponse>, CreateHmacKey,
              (CreateHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteHmacKey,
              (DeleteHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<HmacKeyMetadata>, GetHmacKey, (GetHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<HmacKeyMetadata>, UpdateHmacKey,
              (UpdateHmacKeyRequest const&), (override));
  MOCK_METHOD(StatusOr<SignBlobResponse>, SignBlob, (SignBlobRequest const&),
              (override));
  MOCK_METHOD(StatusOr<ListNotificationsResponse>, ListNotifications,
              (ListNotificationsRequest const&), (override));
  MOCK_METHOD(StatusOr<NotificationMetadata>, CreateNotification,
              (CreateNotificationRequest const&), (override));
  MOCK_METHOD(StatusOr<NotificationMetadata>, GetNotification,
              (GetNotificationRequest const&), (override));
  MOCK_METHOD(StatusOr<EmptyResponse>, DeleteNotification,
              (DeleteNotificationRequest const&), (override));
  // LCOV_EXCL_STOP
};

TEST(StorageConnectionTest, UploadFileSimpleUnimplemented) {
  TestStorageConnection connection;
  InsertObjectMediaRequest request;
  auto response = connection.UploadFileSimple("test-file.txt", 1234, request);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnimplemented));
}

TEST(StorageConnectionTest, UploadFileResumableUnimplemented) {
  TestStorageConnection connection;
  ResumableUploadRequest request;
  auto response = connection.UploadFileResumable("test-file.txt", request);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnimplemented));
}

TEST(StorageConnectionTest, WriteObjectBufferSizeUnimplemented) {
  TestStorageConnection connection;
  ResumableUploadRequest request;
  auto response = connection.WriteObjectBufferSize(request);
  EXPECT_THAT(response, StatusIs(StatusCode::kUnimplemented));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
