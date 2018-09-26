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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_CLIENT_H_

#include "google/cloud/storage/internal/raw_client.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * A decorator for `RawClient` that logs each operation.
 */
class LoggingClient : public RawClient {
 public:
  explicit LoggingClient(std::shared_ptr<RawClient> client);
  ~LoggingClient() override = default;

  ClientOptions const& client_options() const override;

  std::pair<Status, ListBucketsResponse> ListBuckets(
      ListBucketsRequest const& request) override;
  std::pair<Status, BucketMetadata> CreateBucket(
      CreateBucketRequest const& request) override;
  std::pair<Status, BucketMetadata> GetBucketMetadata(
      GetBucketMetadataRequest const& request) override;
  std::pair<Status, EmptyResponse> DeleteBucket(
      DeleteBucketRequest const&) override;
  std::pair<Status, BucketMetadata> UpdateBucket(
      UpdateBucketRequest const& request) override;
  std::pair<Status, BucketMetadata> PatchBucket(
      PatchBucketRequest const& request) override;
  std::pair<Status, IamPolicy> GetBucketIamPolicy(
      GetBucketIamPolicyRequest const& request) override;
  std::pair<Status, IamPolicy> SetBucketIamPolicy(
      SetBucketIamPolicyRequest const& request) override;
  std::pair<Status, TestBucketIamPermissionsResponse> TestBucketIamPermissions(
      TestBucketIamPermissionsRequest const& request) override;

  std::pair<Status, ObjectMetadata> InsertObjectMedia(
      InsertObjectMediaRequest const& request) override;
  std::pair<Status, ObjectMetadata> CopyObject(
      CopyObjectRequest const& request) override;
  std::pair<Status, ObjectMetadata> GetObjectMetadata(
      GetObjectMetadataRequest const& request) override;
  std::pair<Status, std::unique_ptr<ObjectReadStreambuf>> ReadObject(
      ReadObjectRangeRequest const&) override;
  std::pair<Status, std::unique_ptr<ObjectWriteStreambuf>> WriteObject(
      InsertObjectStreamingRequest const&) override;
  std::pair<Status, ListObjectsResponse> ListObjects(
      ListObjectsRequest const&) override;
  std::pair<Status, EmptyResponse> DeleteObject(
      DeleteObjectRequest const&) override;
  std::pair<Status, ObjectMetadata> UpdateObject(
      UpdateObjectRequest const& request) override;
  std::pair<Status, ObjectMetadata> PatchObject(
      PatchObjectRequest const& request) override;
  std::pair<Status, ObjectMetadata> ComposeObject(
      ComposeObjectRequest const& request) override;
  std::pair<Status, RewriteObjectResponse> RewriteObject(
      RewriteObjectRequest const&) override;

  std::pair<Status, ListBucketAclResponse> ListBucketAcl(
      ListBucketAclRequest const& request) override;
  std::pair<Status, BucketAccessControl> CreateBucketAcl(
      CreateBucketAclRequest const&) override;
  std::pair<Status, EmptyResponse> DeleteBucketAcl(
      DeleteBucketAclRequest const&) override;
  std::pair<Status, BucketAccessControl> GetBucketAcl(
      GetBucketAclRequest const&) override;
  std::pair<Status, BucketAccessControl> UpdateBucketAcl(
      UpdateBucketAclRequest const&) override;
  std::pair<Status, BucketAccessControl> PatchBucketAcl(
      PatchBucketAclRequest const&) override;

  std::pair<Status, ListObjectAclResponse> ListObjectAcl(
      ListObjectAclRequest const& request) override;
  std::pair<Status, ObjectAccessControl> CreateObjectAcl(
      CreateObjectAclRequest const&) override;
  std::pair<Status, EmptyResponse> DeleteObjectAcl(
      DeleteObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> GetObjectAcl(
      GetObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> UpdateObjectAcl(
      UpdateObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> PatchObjectAcl(
      PatchObjectAclRequest const&) override;

  std::pair<Status, ListDefaultObjectAclResponse> ListDefaultObjectAcl(
      ListDefaultObjectAclRequest const& request) override;
  std::pair<Status, ObjectAccessControl> CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest const&) override;
  std::pair<Status, EmptyResponse> DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> GetDefaultObjectAcl(
      GetDefaultObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest const&) override;
  std::pair<Status, ObjectAccessControl> PatchDefaultObjectAcl(
      PatchDefaultObjectAclRequest const&) override;

  std::pair<Status, ServiceAccount> GetServiceAccount(
      GetProjectServiceAccountRequest const&) override;

  std::pair<Status, ListNotificationsResponse> ListNotifications(
      ListNotificationsRequest const&) override;
  std::pair<Status, NotificationMetadata> CreateNotification(
      CreateNotificationRequest const&) override;
  std::pair<Status, NotificationMetadata> GetNotification(
      GetNotificationRequest const&) override;
  std::pair<Status, EmptyResponse> DeleteNotification(
      DeleteNotificationRequest const&) override;

  std::shared_ptr<RawClient> client() const { return client_; }

 private:
  std::shared_ptr<RawClient> client_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_CLIENT_H_
