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

#include "google/cloud/storage/idempotency_policy.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

TEST(StrictIdempotencyPolicyTest, ListBuckets) {
  StrictIdempotencyPolicy policy;
  internal::ListBucketsRequest request("test-project");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateBucket) {
  StrictIdempotencyPolicy policy;
  internal::CreateBucketRequest request(
      "test-project", BucketMetadata().set_name("test-bucket-name"));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetBucketMetadata) {
  StrictIdempotencyPolicy policy;
  internal::GetBucketMetadataRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucket) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketRequest request("test-bucket-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucketIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketRequest request("test-bucket-name");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucketIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketRequest request("test-bucket-name");
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucket) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketRequest request(
      BucketMetadata().set_name("test-bucket-name"));
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucketIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketRequest request(
      BucketMetadata().set_name("test-bucket-name"));
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucketIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketRequest request(
      BucketMetadata().set_name("test-bucket-name"));
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucket) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketRequest request("test-bucket-name",
                                       BucketMetadataPatchBuilder());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucketIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketRequest request("test-bucket-name",
                                       BucketMetadataPatchBuilder());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucketIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketRequest request("test-bucket-name",
                                       BucketMetadataPatchBuilder());
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetIamPolicy) {
  StrictIdempotencyPolicy policy;
  internal::GetBucketIamPolicyRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, SetBucketIamPolicy) {
  StrictIdempotencyPolicy policy;
  internal::SetBucketIamPolicyRequest request("test-bucket-name", IamPolicy{});
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, SetBucketIamPolicyIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::SetBucketIamPolicyRequest request("test-bucket-name", IamPolicy{});
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, LockBucketRetentionPolicy) {
  StrictIdempotencyPolicy policy;
  internal::LockBucketRetentionPolicyRequest request("test-bucket-name", 7);
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, InsertObjectMedia) {
  StrictIdempotencyPolicy policy;
  internal::InsertObjectMediaRequest request("test-bucket-name",
                                             "test-object-name", "test-data");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, InsertObjectMediaIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::InsertObjectMediaRequest request("test-bucket-name",
                                             "test-object-name", "test-data");
  request.set_option(IfGenerationMatch(0));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CopyObject) {
  StrictIdempotencyPolicy policy;
  internal::CopyObjectRequest request("test-source-bucket",
                                      "test-source-object", "test-bucket-name",
                                      "test-object-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CopyObjectIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::CopyObjectRequest request("test-source-bucket",
                                      "test-source-object", "test-bucket-name",
                                      "test-object-name");
  request.set_option(IfGenerationMatch(0));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetObjectMetadata) {
  StrictIdempotencyPolicy policy;
  internal::GetObjectMetadataRequest request("test-bucket-name",
                                             "test-object-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ReadObject) {
  StrictIdempotencyPolicy policy;
  internal::ReadObjectRangeRequest request("test-bucket-name",
                                           "test-object-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, WriteObject) {
  StrictIdempotencyPolicy policy;
  internal::InsertObjectStreamingRequest request("test-bucket-name",
                                                 "test-object-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, WriteObjectIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::InsertObjectStreamingRequest request("test-bucket-name",
                                                 "test-object-name");
  request.set_option(IfGenerationMatch(0));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ListObjects) {
  StrictIdempotencyPolicy policy;
  internal::ListObjectsRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteObject) {
  StrictIdempotencyPolicy policy;
  internal::DeleteObjectRequest request("test-bucket-name", "test-object-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteObjectIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::DeleteObjectRequest request("test-bucket-name", "test-object-name");
  request.set_option(IfGenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteObjectGeneration) {
  StrictIdempotencyPolicy policy;
  internal::DeleteObjectRequest request("test-bucket-name", "test-object-name");
  request.set_option(Generation(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateObject) {
  StrictIdempotencyPolicy policy;
  internal::UpdateObjectRequest request("test-bucket-name", "test-object-name",
                                        ObjectMetadata());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateObjectIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateObjectRequest request("test-bucket-name", "test-object-name",
                                        ObjectMetadata());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateObjectIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::UpdateObjectRequest request("test-bucket-name", "test-object-name",
                                        ObjectMetadata());
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchObject) {
  StrictIdempotencyPolicy policy;
  internal::PatchObjectRequest request("test-bucket-name", "test-object-name",
                                       ObjectMetadataPatchBuilder());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchObjectIfEtag) {
  StrictIdempotencyPolicy policy;
  internal::PatchObjectRequest request("test-bucket-name", "test-object-name",
                                       ObjectMetadataPatchBuilder());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchObjectIfMetagenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::PatchObjectRequest request("test-bucket-name", "test-object-name",
                                       ObjectMetadataPatchBuilder());
  request.set_option(IfMetagenerationMatch(7));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ComposeObject) {
  StrictIdempotencyPolicy policy;
  internal::ComposeObjectRequest request("test-bucket-name",
                                         {ComposeSourceObject{"source-1"}},
                                         "test-object-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ComposeObjectIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::CopyObjectRequest request("test-source-bucket",
                                      "test-source-object", "test-bucket-name",
                                      "test-object-name");
  request.set_option(IfGenerationMatch(0));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, RewriteObject) {
  StrictIdempotencyPolicy policy;
  internal::RewriteObjectRequest request(
      "test-source-bucket", "test-source-object", "test-bucket-name",
      "test-object-name", std::string{});
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, RewriteObjectIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::RewriteObjectRequest request(
      "test-source-bucket", "test-source-object", "test-bucket-name",
      "test-object-name", std::string{});
  request.set_option(IfGenerationMatch(0));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ListBucketAcl) {
  StrictIdempotencyPolicy policy;
  internal::ListBucketAclRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateBucketAcl) {
  StrictIdempotencyPolicy policy;
  internal::CreateBucketAclRequest request("test-bucket-name",
                                           "test-entity-name", "READER");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateBucketAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::CreateBucketAclRequest request("test-bucket-name",
                                           "test-entity-name", "READER");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucketAcl) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketAclRequest request("test-bucket-name",
                                           "test-entity-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteBucketAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::DeleteBucketAclRequest request("test-bucket-name",
                                           "test-entity-name");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetBucketAcl) {
  StrictIdempotencyPolicy policy;
  internal::GetBucketAclRequest request("test-bucket-name", "test-entity-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucketAcl) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketAclRequest request("test-bucket-name",
                                           "test-entity-name", "READER");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateBucketAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateBucketAclRequest request("test-bucket-name",
                                           "test-entity-name", "READER");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucketAcl) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketAclRequest request("test-bucket-name",
                                          "test-entity-name",
                                          BucketAccessControlPatchBuilder());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchBucketAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::PatchBucketAclRequest request("test-bucket-name",
                                          "test-entity-name",
                                          BucketAccessControlPatchBuilder());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ListObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::ListObjectAclRequest request("test-bucket-name",
                                         "test-object-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::CreateObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name", "READER");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::CreateObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name", "READER");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::DeleteObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::DeleteObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::GetObjectAclRequest request("test-bucket-name", "test-object-name",
                                        "test-entity-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::UpdateObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name", "READER");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name", "READER");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::PatchObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name",
      ObjectAccessControlPatchBuilder());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::PatchObjectAclRequest request(
      "test-bucket-name", "test-object-name", "test-entity-name",
      ObjectAccessControlPatchBuilder());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ListDefaultObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::ListDefaultObjectAclRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateDefaultObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::CreateDefaultObjectAclRequest request("test-bucket-name",
                                                  "test-entity-name", "READER");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateDefaultObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::CreateDefaultObjectAclRequest request("test-bucket-name",
                                                  "test-entity-name", "READER");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteDefaultObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::DeleteDefaultObjectAclRequest request("test-bucket-name",
                                                  "test-entity-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteDefaultObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::DeleteDefaultObjectAclRequest request("test-bucket-name",
                                                  "test-entity-name");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetDefaultObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::GetDefaultObjectAclRequest request("test-bucket-name",
                                               "test-entity-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateDefaultObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::UpdateDefaultObjectAclRequest request("test-bucket-name",
                                                  "test-entity-name", "READER");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateDefaultObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateDefaultObjectAclRequest request("test-bucket-name",
                                                  "test-entity-name", "READER");
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchDefaultObjectAcl) {
  StrictIdempotencyPolicy policy;
  internal::PatchDefaultObjectAclRequest request(
      "test-bucket-name", "test-entity-name",
      ObjectAccessControlPatchBuilder());
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, PatchDefaultObjectAclIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::PatchDefaultObjectAclRequest request(
      "test-bucket-name", "test-entity-name",
      ObjectAccessControlPatchBuilder());
  request.set_option(IfMatchEtag("ABC123="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetProjectServiceAccount) {
  StrictIdempotencyPolicy policy;
  internal::GetProjectServiceAccountRequest request("test-project-id");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ListHmacKeys) {
  StrictIdempotencyPolicy policy;
  internal::ListHmacKeysRequest request("test-project-id");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateHmacKey) {
  StrictIdempotencyPolicy policy;
  internal::CreateHmacKeyRequest request("test-project-id",
                                         "test-service-account");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteHmacKey) {
  StrictIdempotencyPolicy policy;
  internal::DeleteHmacKeyRequest request("test-project-id", "test-access-id");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetHmacKey) {
  StrictIdempotencyPolicy policy;
  internal::GetHmacKeyRequest request("test-project-id", "test-access-id");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateHmacKey) {
  StrictIdempotencyPolicy policy;
  internal::UpdateHmacKeyRequest request(
      "test-project-id", "test-access-id",
      HmacKeyMetadata().set_state("INACTIVE"));
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateHmacKeyWithEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateHmacKeyRequest request(
      "test-project-id", "test-access-id",
      HmacKeyMetadata().set_state("INACTIVE").set_etag("ABC="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UpdateHmacKeyIfMatchEtag) {
  StrictIdempotencyPolicy policy;
  internal::UpdateHmacKeyRequest request(
      "test-project-id", "test-access-id",
      HmacKeyMetadata().set_state("INACTIVE"));
  request.set_multiple_options(IfMatchEtag("ABC="));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, SignBlob) {
  StrictIdempotencyPolicy policy;
  internal::SignBlobRequest request("test-key-id", "test-blob", {});
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ListNotification) {
  StrictIdempotencyPolicy policy;
  internal::ListNotificationsRequest request("test-bucket-name");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, CreateNotification) {
  StrictIdempotencyPolicy policy;
  internal::CreateNotificationRequest request("test-bucket-name",
                                              NotificationMetadata());
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, GetNotification) {
  StrictIdempotencyPolicy policy;
  internal::GetNotificationRequest request("test-bucket-name",
                                           "test-notification-id");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, DeleteNotification) {
  StrictIdempotencyPolicy policy;
  internal::DeleteNotificationRequest request("test-bucket-name",
                                              "test-notification-id");
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ResumableUpload) {
  StrictIdempotencyPolicy policy;
  internal::ResumableUploadRequest request("test-bucket-name",
                                           "test-object-name");
  EXPECT_FALSE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, ResumableUploadIfGenerationMatch) {
  StrictIdempotencyPolicy policy;
  internal::ResumableUploadRequest request("test-bucket-name",
                                           "test-object-name");
  request.set_option(IfGenerationMatch(0));
  EXPECT_TRUE(policy.IsIdempotent(request));
}

TEST(StrictIdempotencyPolicyTest, UploadChunk) {
  StrictIdempotencyPolicy policy;
  internal::UploadChunkRequest request("https://test-url.example.com", 0,
                                       "test-payload", false);
  EXPECT_TRUE(policy.IsIdempotent(request));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
