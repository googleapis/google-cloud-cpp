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

#include "google/cloud/storage/internal/generic_stub_adapter.h"
#include "google/cloud/storage/internal/generic_stub.h"
#include "google/cloud/storage/internal/storage_connection.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class GenericStubAdapter : public GenericStub {
 public:
  explicit GenericStubAdapter(
      std::shared_ptr<storage::internal::StorageConnection> impl)
      : impl_(std::move(impl)) {}
  ~GenericStubAdapter() noexcept override = default;

  google::cloud::Options options() const override { return impl_->options(); }

  StatusOr<storage::internal::ListBucketsResponse> ListBuckets(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListBucketsRequest const& request) override {
    return impl_->ListBuckets(request);
  }
  StatusOr<storage::BucketMetadata> CreateBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateBucketRequest const& request) override {
    return impl_->CreateBucket(request);
  }
  StatusOr<storage::BucketMetadata> GetBucketMetadata(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetBucketMetadataRequest const& request) override {
    return impl_->GetBucketMetadata(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteBucketRequest const& request) override {
    return impl_->DeleteBucket(request);
  }
  StatusOr<storage::BucketMetadata> UpdateBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateBucketRequest const& request) override {
    return impl_->UpdateBucket(request);
  }
  StatusOr<storage::BucketMetadata> PatchBucket(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchBucketRequest const& request) override {
    return impl_->PatchBucket(request);
  }
  StatusOr<storage::NativeIamPolicy> GetNativeBucketIamPolicy(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetBucketIamPolicyRequest const& request) override {
    return impl_->GetNativeBucketIamPolicy(request);
  }
  StatusOr<storage::NativeIamPolicy> SetNativeBucketIamPolicy(
      rest_internal::RestContext&, Options const&,
      storage::internal::SetNativeBucketIamPolicyRequest const& request)
      override {
    return impl_->SetNativeBucketIamPolicy(request);
  }
  StatusOr<storage::internal::TestBucketIamPermissionsResponse>
  TestBucketIamPermissions(
      rest_internal::RestContext&, Options const&,
      storage::internal::TestBucketIamPermissionsRequest const& request)
      override {
    return impl_->TestBucketIamPermissions(request);
  }
  StatusOr<storage::BucketMetadata> LockBucketRetentionPolicy(
      rest_internal::RestContext&, Options const&,
      storage::internal::LockBucketRetentionPolicyRequest const& request)
      override {
    return impl_->LockBucketRetentionPolicy(request);
  }

  StatusOr<storage::ObjectMetadata> InsertObjectMedia(
      rest_internal::RestContext&, Options const&,
      storage::internal::InsertObjectMediaRequest const& request) override {
    return impl_->InsertObjectMedia(request);
  }
  StatusOr<storage::ObjectMetadata> CopyObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::CopyObjectRequest const& request) override {
    return impl_->CopyObject(request);
  }
  StatusOr<storage::ObjectMetadata> GetObjectMetadata(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetObjectMetadataRequest const& request) override {
    return impl_->GetObjectMetadata(request);
  }
  StatusOr<std::unique_ptr<storage::internal::ObjectReadSource>> ReadObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::ReadObjectRangeRequest const& request) override {
    return impl_->ReadObject(request);
  }
  StatusOr<storage::internal::ListObjectsResponse> ListObjects(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListObjectsRequest const& request) override {
    return impl_->ListObjects(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteObjectRequest const& request) override {
    return impl_->DeleteObject(request);
  }
  StatusOr<storage::ObjectMetadata> UpdateObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateObjectRequest const& request) override {
    return impl_->UpdateObject(request);
  }
  StatusOr<storage::ObjectMetadata> MoveObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::MoveObjectRequest const& request) override {
    return impl_->MoveObject(request);
  }
  StatusOr<storage::ObjectMetadata> PatchObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchObjectRequest const& request) override {
    return impl_->PatchObject(request);
  }
  StatusOr<storage::ObjectMetadata> ComposeObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::ComposeObjectRequest const& request) override {
    return impl_->ComposeObject(request);
  }
  StatusOr<storage::internal::RewriteObjectResponse> RewriteObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::RewriteObjectRequest const& request) override {
    return impl_->RewriteObject(request);
  }
  StatusOr<storage::ObjectMetadata> RestoreObject(
      rest_internal::RestContext&, Options const&,
      storage::internal::RestoreObjectRequest const& request) override {
    return impl_->RestoreObject(request);
  }

  StatusOr<storage::internal::CreateResumableUploadResponse>
  CreateResumableUpload(
      rest_internal::RestContext&, Options const&,
      storage::internal::ResumableUploadRequest const& request) override {
    return impl_->CreateResumableUpload(request);
  }
  StatusOr<storage::internal::QueryResumableUploadResponse>
  QueryResumableUpload(
      rest_internal::RestContext&, Options const&,
      storage::internal::QueryResumableUploadRequest const& request) override {
    return impl_->QueryResumableUpload(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteResumableUpload(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteResumableUploadRequest const& request) override {
    return impl_->DeleteResumableUpload(request);
  }
  StatusOr<storage::internal::QueryResumableUploadResponse> UploadChunk(
      rest_internal::RestContext&, Options const&,
      storage::internal::UploadChunkRequest const& request) override {
    return impl_->UploadChunk(request);
  }

  StatusOr<storage::internal::ListBucketAclResponse> ListBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListBucketAclRequest const& request) override {
    return impl_->ListBucketAcl(request);
  }
  StatusOr<storage::BucketAccessControl> CreateBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateBucketAclRequest const& request) override {
    return impl_->CreateBucketAcl(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteBucketAclRequest const& request) override {
    return impl_->DeleteBucketAcl(request);
  }
  StatusOr<storage::BucketAccessControl> GetBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetBucketAclRequest const& request) override {
    return impl_->GetBucketAcl(request);
  }
  StatusOr<storage::BucketAccessControl> UpdateBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateBucketAclRequest const& request) override {
    return impl_->UpdateBucketAcl(request);
  }
  StatusOr<storage::BucketAccessControl> PatchBucketAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchBucketAclRequest const& request) override {
    return impl_->PatchBucketAcl(request);
  }

  StatusOr<storage::internal::ListObjectAclResponse> ListObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListObjectAclRequest const& request) override {
    return impl_->ListObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> CreateObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateObjectAclRequest const& request) override {
    return impl_->CreateObjectAcl(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteObjectAclRequest const& request) override {
    return impl_->DeleteObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> GetObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetObjectAclRequest const& request) override {
    return impl_->GetObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> UpdateObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateObjectAclRequest const& request) override {
    return impl_->UpdateObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> PatchObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchObjectAclRequest const& request) override {
    return impl_->PatchObjectAcl(request);
  }

  StatusOr<storage::internal::ListDefaultObjectAclResponse>
  ListDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListDefaultObjectAclRequest const& request) override {
    return impl_->ListDefaultObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> CreateDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateDefaultObjectAclRequest const& request)
      override {
    return impl_->CreateDefaultObjectAcl(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteDefaultObjectAclRequest const& request)
      override {
    return impl_->DeleteDefaultObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> GetDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetDefaultObjectAclRequest const& request) override {
    return impl_->GetDefaultObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> UpdateDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateDefaultObjectAclRequest const& request)
      override {
    return impl_->UpdateDefaultObjectAcl(request);
  }
  StatusOr<storage::ObjectAccessControl> PatchDefaultObjectAcl(
      rest_internal::RestContext&, Options const&,
      storage::internal::PatchDefaultObjectAclRequest const& request) override {
    return impl_->PatchDefaultObjectAcl(request);
  }

  StatusOr<storage::ServiceAccount> GetServiceAccount(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetProjectServiceAccountRequest const& request)
      override {
    return impl_->GetServiceAccount(request);
  }
  StatusOr<storage::internal::ListHmacKeysResponse> ListHmacKeys(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListHmacKeysRequest const& request) override {
    return impl_->ListHmacKeys(request);
  }
  StatusOr<storage::internal::CreateHmacKeyResponse> CreateHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateHmacKeyRequest const& request) override {
    return impl_->CreateHmacKey(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteHmacKeyRequest const& request) override {
    return impl_->DeleteHmacKey(request);
  }
  StatusOr<storage::HmacKeyMetadata> GetHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetHmacKeyRequest const& request) override {
    return impl_->GetHmacKey(request);
  }
  StatusOr<storage::HmacKeyMetadata> UpdateHmacKey(
      rest_internal::RestContext&, Options const&,
      storage::internal::UpdateHmacKeyRequest const& request) override {
    return impl_->UpdateHmacKey(request);
  }
  StatusOr<storage::internal::SignBlobResponse> SignBlob(
      rest_internal::RestContext&, Options const&,
      storage::internal::SignBlobRequest const& request) override {
    return impl_->SignBlob(request);
  }

  StatusOr<storage::internal::ListNotificationsResponse> ListNotifications(
      rest_internal::RestContext&, Options const&,
      storage::internal::ListNotificationsRequest const& request) override {
    return impl_->ListNotifications(request);
  }
  StatusOr<storage::NotificationMetadata> CreateNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::CreateNotificationRequest const& request) override {
    return impl_->CreateNotification(request);
  }
  StatusOr<storage::NotificationMetadata> GetNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::GetNotificationRequest const& request) override {
    return impl_->GetNotification(request);
  }
  StatusOr<storage::internal::EmptyResponse> DeleteNotification(
      rest_internal::RestContext&, Options const&,
      storage::internal::DeleteNotificationRequest const& request) override {
    return impl_->DeleteNotification(request);
  }

  std::vector<std::string> InspectStackStructure() const override {
    auto stack = impl_->InspectStackStructure();
    stack.emplace_back("GenericStubAdapter");
    return stack;
  }

 private:
  std::shared_ptr<storage::internal::StorageConnection> impl_;
};

}  // namespace

std::unique_ptr<GenericStub> MakeGenericStubAdapter(
    std::shared_ptr<storage::internal::StorageConnection> impl) {
  return std::make_unique<GenericStubAdapter>(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
