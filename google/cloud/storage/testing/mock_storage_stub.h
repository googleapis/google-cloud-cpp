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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H

#include "google/cloud/storage/internal/storage_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockStorageStub : public internal::StorageStub {
 public:
  MOCK_METHOD(std::unique_ptr<ObjectMediaStream>, GetObjectMedia,
              (std::unique_ptr<grpc::ClientContext>,
               google::storage::v1::GetObjectMediaRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<InsertStream>, InsertObjectMedia,
              (std::unique_ptr<grpc::ClientContext>), (override));
  MOCK_METHOD(Status, DeleteBucketAccessControl,
              (grpc::ClientContext&,
               google::storage::v1::DeleteBucketAccessControlRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::BucketAccessControl>,
              GetBucketAccessControl,
              (grpc::ClientContext&,
               google::storage::v1::GetBucketAccessControlRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::BucketAccessControl>,
              InsertBucketAccessControl,
              (grpc::ClientContext&,
               google::storage::v1::InsertBucketAccessControlRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::ListBucketAccessControlsResponse>,
              ListBucketAccessControls,
              (grpc::ClientContext&,
               google::storage::v1::ListBucketAccessControlsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::BucketAccessControl>,
              UpdateBucketAccessControl,
              (grpc::ClientContext&,
               google::storage::v1::UpdateBucketAccessControlRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteBucket,
              (grpc::ClientContext&,
               google::storage::v1::DeleteBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::Bucket>, GetBucket,
              (grpc::ClientContext&,
               google::storage::v1::GetBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::Bucket>, InsertBucket,
              (grpc::ClientContext&,
               google::storage::v1::InsertBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::ListBucketsResponse>, ListBuckets,
              (grpc::ClientContext&,
               google::storage::v1::ListBucketsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetBucketIamPolicy,
              (grpc::ClientContext&,
               google::storage::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetBucketIamPolicy,
              (grpc::ClientContext&,
               google::storage::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestBucketIamPermissions,
              (grpc::ClientContext&,
               google::storage::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::Bucket>, UpdateBucket,
              (grpc::ClientContext&,
               google::storage::v1::UpdateBucketRequest const&),
              (override));
  MOCK_METHOD(
      Status, DeleteDefaultObjectAccessControl,
      (grpc::ClientContext&,
       google::storage::v1::DeleteDefaultObjectAccessControlRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::storage::v1::ObjectAccessControl>,
      GetDefaultObjectAccessControl,
      (grpc::ClientContext&,
       google::storage::v1::GetDefaultObjectAccessControlRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::storage::v1::ObjectAccessControl>,
      InsertDefaultObjectAccessControl,
      (grpc::ClientContext&,
       google::storage::v1::InsertDefaultObjectAccessControlRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::storage::v1::ListObjectAccessControlsResponse>,
      ListDefaultObjectAccessControls,
      (grpc::ClientContext&,
       google::storage::v1::ListDefaultObjectAccessControlsRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::storage::v1::ObjectAccessControl>,
      UpdateDefaultObjectAccessControl,
      (grpc::ClientContext&,
       google::storage::v1::UpdateDefaultObjectAccessControlRequest const&),
      (override));
  MOCK_METHOD(Status, DeleteNotification,
              (grpc::ClientContext&,
               google::storage::v1::DeleteNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::Notification>, GetNotification,
              (grpc::ClientContext&,
               google::storage::v1::GetNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::Notification>, InsertNotification,
              (grpc::ClientContext&,
               google::storage::v1::InsertNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::ListNotificationsResponse>,
              ListNotifications,
              (grpc::ClientContext&,
               google::storage::v1::ListNotificationsRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteObject,
              (grpc::ClientContext&,
               google::storage::v1::DeleteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::StartResumableWriteResponse>,
              StartResumableWrite,
              (grpc::ClientContext&,
               google::storage::v1::StartResumableWriteRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v1::QueryWriteStatusResponse>,
              QueryWriteStatus,
              (grpc::ClientContext&,
               google::storage::v1::QueryWriteStatusRequest const&),
              (override));
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
