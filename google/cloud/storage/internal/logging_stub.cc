// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/logging_stub.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {

// We cannot use the `google::cloud::internal::LogWrapper` functions because
// they depend on `DebugString()` working, and introduce dependencies on gRPC
// and Protobuf.
template <typename Functor, typename Request>
auto LogWrapper(Functor&& functor,
                google::cloud::rest_internal::RestContext& context,
                google::cloud::Options const& options, Request const& request,
                char const* where)
    -> google::cloud::internal::invoke_result_t<
        Functor, google::cloud::rest_internal::RestContext&,
        google::cloud::Options const&, Request const&> {
  GCP_LOG(INFO) << where << "() << " << request;
  auto response = functor(context, options, request);
  if (response.ok()) {
    GCP_LOG(INFO) << where << "() >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << where << "() >> status={" << response.status() << "}";
  }
  return response;
}

}  // namespace

LoggingStub::LoggingStub(std::unique_ptr<storage_internal::GenericStub> stub)
    : stub_(std::move(stub)) {}

Options LoggingStub::options() const { return stub_->options(); }

StatusOr<ListBucketsResponse> LoggingStub::ListBuckets(
    rest_internal::RestContext& context, Options const& options,
    ListBucketsRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListBuckets(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketMetadata> LoggingStub::CreateBucket(
    rest_internal::RestContext& context, Options const& options,
    CreateBucketRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateBucket(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketMetadata> LoggingStub::GetBucketMetadata(
    rest_internal::RestContext& context, Options const& options,
    GetBucketMetadataRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetBucketMetadata(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteBucket(
    rest_internal::RestContext& context, Options const& options,
    DeleteBucketRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteBucket(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketMetadata> LoggingStub::UpdateBucket(
    rest_internal::RestContext& context, Options const& options,
    UpdateBucketRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UpdateBucket(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketMetadata> LoggingStub::PatchBucket(
    rest_internal::RestContext& context, Options const& options,
    PatchBucketRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->PatchBucket(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<NativeIamPolicy> LoggingStub::GetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    GetBucketIamPolicyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetNativeBucketIamPolicy(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<NativeIamPolicy> LoggingStub::SetNativeBucketIamPolicy(
    rest_internal::RestContext& context, Options const& options,
    SetNativeBucketIamPolicyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->SetNativeBucketIamPolicy(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<TestBucketIamPermissionsResponse>
LoggingStub::TestBucketIamPermissions(
    rest_internal::RestContext& context, Options const& options,
    TestBucketIamPermissionsRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->TestBucketIamPermissions(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketMetadata> LoggingStub::LockBucketRetentionPolicy(
    rest_internal::RestContext& context, Options const& options,
    LockBucketRetentionPolicyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->LockBucketRetentionPolicy(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectMetadata> LoggingStub::InsertObjectMedia(
    rest_internal::RestContext& context, Options const& options,
    InsertObjectMediaRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->InsertObjectMedia(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectMetadata> LoggingStub::CopyObject(
    rest_internal::RestContext& context, Options const& options,
    CopyObjectRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CopyObject(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectMetadata> LoggingStub::GetObjectMetadata(
    rest_internal::RestContext& context, Options const& options,
    GetObjectMetadataRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetObjectMetadata(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<std::unique_ptr<ObjectReadSource>> LoggingStub::ReadObject(
    rest_internal::RestContext& context, Options const& options,
    ReadObjectRangeRequest const& request) {
  GCP_LOG(INFO) << __func__ << "() << " << request;
  return stub_->ReadObject(context, options, request);
}

StatusOr<ListObjectsResponse> LoggingStub::ListObjects(
    rest_internal::RestContext& context, Options const& options,
    ListObjectsRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListObjects(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteObject(
    rest_internal::RestContext& context, Options const& options,
    DeleteObjectRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteObject(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectMetadata> LoggingStub::UpdateObject(
    rest_internal::RestContext& context, Options const& options,
    UpdateObjectRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UpdateObject(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectMetadata> LoggingStub::PatchObject(
    rest_internal::RestContext& context, Options const& options,
    PatchObjectRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->PatchObject(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectMetadata> LoggingStub::ComposeObject(
    rest_internal::RestContext& context, Options const& options,
    ComposeObjectRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ComposeObject(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<RewriteObjectResponse> LoggingStub::RewriteObject(
    rest_internal::RestContext& context, Options const& options,
    RewriteObjectRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->RewriteObject(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<CreateResumableUploadResponse> LoggingStub::CreateResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    ResumableUploadRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateResumableUpload(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<QueryResumableUploadResponse> LoggingStub::QueryResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    QueryResumableUploadRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->QueryResumableUpload(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteResumableUpload(
    rest_internal::RestContext& context, Options const& options,
    DeleteResumableUploadRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteResumableUpload(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<QueryResumableUploadResponse> LoggingStub::UploadChunk(
    rest_internal::RestContext& context, Options const& options,
    UploadChunkRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UploadChunk(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ListBucketAclResponse> LoggingStub::ListBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    ListBucketAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListBucketAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketAccessControl> LoggingStub::GetBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    GetBucketAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetBucketAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketAccessControl> LoggingStub::CreateBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    CreateBucketAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateBucketAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    DeleteBucketAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteBucketAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketAccessControl> LoggingStub::UpdateBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    UpdateBucketAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UpdateBucketAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<BucketAccessControl> LoggingStub::PatchBucketAcl(
    rest_internal::RestContext& context, Options const& options,
    PatchBucketAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->PatchBucketAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ListObjectAclResponse> LoggingStub::ListObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    ListObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::CreateObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    CreateObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    DeleteObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::GetObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    GetObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::UpdateObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    UpdateObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UpdateObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::PatchObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    PatchObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->PatchObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ListDefaultObjectAclResponse> LoggingStub::ListDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    ListDefaultObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListDefaultObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::CreateDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    CreateDefaultObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateDefaultObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    DeleteDefaultObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteDefaultObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::GetDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    GetDefaultObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetDefaultObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::UpdateDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    UpdateDefaultObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UpdateDefaultObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ObjectAccessControl> LoggingStub::PatchDefaultObjectAcl(
    rest_internal::RestContext& context, Options const& options,
    PatchDefaultObjectAclRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->PatchDefaultObjectAcl(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ServiceAccount> LoggingStub::GetServiceAccount(
    rest_internal::RestContext& context, Options const& options,
    GetProjectServiceAccountRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetServiceAccount(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ListHmacKeysResponse> LoggingStub::ListHmacKeys(
    rest_internal::RestContext& context, Options const& options,
    ListHmacKeysRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListHmacKeys(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<CreateHmacKeyResponse> LoggingStub::CreateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    CreateHmacKeyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateHmacKey(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteHmacKey(
    rest_internal::RestContext& context, Options const& options,
    DeleteHmacKeyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteHmacKey(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<HmacKeyMetadata> LoggingStub::GetHmacKey(
    rest_internal::RestContext& context, Options const& options,
    GetHmacKeyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetHmacKey(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<HmacKeyMetadata> LoggingStub::UpdateHmacKey(
    rest_internal::RestContext& context, Options const& options,
    UpdateHmacKeyRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->UpdateHmacKey(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<SignBlobResponse> LoggingStub::SignBlob(
    rest_internal::RestContext& context, Options const& options,
    SignBlobRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->SignBlob(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<ListNotificationsResponse> LoggingStub::ListNotifications(
    rest_internal::RestContext& context, Options const& options,
    ListNotificationsRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->ListNotifications(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<NotificationMetadata> LoggingStub::CreateNotification(
    rest_internal::RestContext& context, Options const& options,
    CreateNotificationRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->CreateNotification(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<NotificationMetadata> LoggingStub::GetNotification(
    rest_internal::RestContext& context, Options const& options,
    GetNotificationRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->GetNotification(context, options, request);
      },
      context, options, request, __func__);
}

StatusOr<EmptyResponse> LoggingStub::DeleteNotification(
    rest_internal::RestContext& context, Options const& options,
    DeleteNotificationRequest const& request) {
  return LogWrapper(
      [this](auto& context, auto const& options, auto& request) {
        return stub_->DeleteNotification(context, options, request);
      },
      context, options, request, __func__);
}

std::vector<std::string> LoggingStub::InspectStackStructure() const {
  auto stack = stub_->InspectStackStructure();
  stack.emplace_back("LoggingStub");
  return stack;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
