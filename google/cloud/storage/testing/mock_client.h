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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockClient : public google::cloud::storage::internal::RawClient {
 public:
  // The MOCK_* macros get confused if the return type is a compound template
  // with a comma, that is because Foo<T,R> looks like two arguments to the
  // preprocessor, but Foo<R> will look like a single argument.
  template <typename R>
  using ResponseWrapper = std::pair<google::cloud::storage::Status, R>;

  MOCK_CONST_METHOD0(client_options, ClientOptions const&());
  MOCK_METHOD1(ListBuckets, ResponseWrapper<internal::ListBucketsResponse>(
                                internal::ListBucketsRequest const&));
  MOCK_METHOD1(CreateBucket, ResponseWrapper<storage::BucketMetadata>(
                                 internal::CreateBucketRequest const&));
  MOCK_METHOD1(GetBucketMetadata,
               ResponseWrapper<storage::BucketMetadata>(
                   internal::GetBucketMetadataRequest const&));
  MOCK_METHOD1(DeleteBucket, ResponseWrapper<internal::EmptyResponse>(
                                 internal::DeleteBucketRequest const&));
  MOCK_METHOD1(UpdateBucket, ResponseWrapper<storage::BucketMetadata>(
                                 internal::UpdateBucketRequest const&));
  MOCK_METHOD1(PatchBucket, ResponseWrapper<storage::BucketMetadata>(
                                internal::PatchBucketRequest const&));
  MOCK_METHOD1(
      GetBucketIamPolicy,
      ResponseWrapper<IamPolicy>(internal::GetBucketIamPolicyRequest const&));
  MOCK_METHOD1(
      SetBucketIamPolicy,
      ResponseWrapper<IamPolicy>(internal::SetBucketIamPolicyRequest const&));
  MOCK_METHOD1(TestBucketIamPermissions,
               ResponseWrapper<internal::TestBucketIamPermissionsResponse>(
                   internal::TestBucketIamPermissionsRequest const&));

  MOCK_METHOD1(InsertObjectMedia,
               ResponseWrapper<storage::ObjectMetadata>(
                   internal::InsertObjectMediaRequest const&));
  MOCK_METHOD1(CopyObject, ResponseWrapper<storage::ObjectMetadata>(
                               internal::CopyObjectRequest const&));
  MOCK_METHOD1(GetObjectMetadata,
               ResponseWrapper<storage::ObjectMetadata>(
                   internal::GetObjectMetadataRequest const&));
  MOCK_METHOD1(ReadObject,
               ResponseWrapper<std::unique_ptr<internal::ObjectReadStreambuf>>(
                   internal::ReadObjectRangeRequest const&));
  MOCK_METHOD1(WriteObject,
               ResponseWrapper<std::unique_ptr<internal::ObjectWriteStreambuf>>(
                   internal::InsertObjectStreamingRequest const&));
  MOCK_METHOD1(ListObjects, ResponseWrapper<internal::ListObjectsResponse>(
                                internal::ListObjectsRequest const&));
  MOCK_METHOD1(DeleteObject, ResponseWrapper<internal::EmptyResponse>(
                                 internal::DeleteObjectRequest const&));
  MOCK_METHOD1(UpdateObject, ResponseWrapper<storage::ObjectMetadata>(
                                 internal::UpdateObjectRequest const&));
  MOCK_METHOD1(PatchObject, ResponseWrapper<storage::ObjectMetadata>(
                                internal::PatchObjectRequest const&));
  MOCK_METHOD1(ComposeObject, ResponseWrapper<storage::ObjectMetadata>(
                                  internal::ComposeObjectRequest const&));
  MOCK_METHOD1(RewriteObject, ResponseWrapper<internal::RewriteObjectResponse>(
                                  internal::RewriteObjectRequest const&));

  MOCK_METHOD1(ListBucketAcl, ResponseWrapper<internal::ListBucketAclResponse>(
                                  internal::ListBucketAclRequest const&));
  MOCK_METHOD1(CreateBucketAcl, ResponseWrapper<BucketAccessControl>(
                                    internal::CreateBucketAclRequest const&));
  MOCK_METHOD1(DeleteBucketAcl, ResponseWrapper<internal::EmptyResponse>(
                                    internal::DeleteBucketAclRequest const&));
  MOCK_METHOD1(GetBucketAcl, ResponseWrapper<BucketAccessControl>(
                                 internal::GetBucketAclRequest const&));
  MOCK_METHOD1(UpdateBucketAcl, ResponseWrapper<BucketAccessControl>(
                                    internal::UpdateBucketAclRequest const&));
  MOCK_METHOD1(PatchBucketAcl, ResponseWrapper<BucketAccessControl>(
                                   internal::PatchBucketAclRequest const&));

  MOCK_METHOD1(ListObjectAcl, ResponseWrapper<internal::ListObjectAclResponse>(
                                  internal::ListObjectAclRequest const&));
  MOCK_METHOD1(CreateObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                    internal::CreateObjectAclRequest const&));
  MOCK_METHOD1(DeleteObjectAcl, ResponseWrapper<internal::EmptyResponse>(
                                    internal::DeleteObjectAclRequest const&));
  MOCK_METHOD1(GetObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                 internal::GetObjectAclRequest const&));
  MOCK_METHOD1(UpdateObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                    internal::UpdateObjectAclRequest const&));
  MOCK_METHOD1(PatchObjectAcl, ResponseWrapper<ObjectAccessControl>(
                                   internal::PatchObjectAclRequest const&));

  MOCK_METHOD1(ListDefaultObjectAcl,
               ResponseWrapper<internal::ListDefaultObjectAclResponse>(
                   internal::ListDefaultObjectAclRequest const&));
  MOCK_METHOD1(CreateDefaultObjectAcl,
               ResponseWrapper<ObjectAccessControl>(
                   internal::CreateDefaultObjectAclRequest const&));
  MOCK_METHOD1(DeleteDefaultObjectAcl,
               ResponseWrapper<internal::EmptyResponse>(
                   internal::DeleteDefaultObjectAclRequest const&));
  MOCK_METHOD1(GetDefaultObjectAcl,
               ResponseWrapper<ObjectAccessControl>(
                   internal::GetDefaultObjectAclRequest const&));
  MOCK_METHOD1(UpdateDefaultObjectAcl,
               ResponseWrapper<ObjectAccessControl>(
                   internal::UpdateDefaultObjectAclRequest const&));
  MOCK_METHOD1(PatchDefaultObjectAcl,
               ResponseWrapper<ObjectAccessControl>(
                   internal::PatchDefaultObjectAclRequest const&));

  MOCK_METHOD1(GetServiceAccount,
               ResponseWrapper<ServiceAccount>(
                   internal::GetProjectServiceAccountRequest const&));

  MOCK_METHOD1(ListNotifications,
               ResponseWrapper<internal::ListNotificationsResponse>(
                   internal::ListNotificationsRequest const&));
  MOCK_METHOD1(CreateNotification,
               ResponseWrapper<NotificationMetadata>(
                   internal::CreateNotificationRequest const&));
  MOCK_METHOD1(GetNotification, ResponseWrapper<NotificationMetadata>(
                                    internal::GetNotificationRequest const&));
  MOCK_METHOD1(DeleteNotification,
               ResponseWrapper<internal::EmptyResponse>(
                   internal::DeleteNotificationRequest const&));
};
}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_CLIENT_H_
