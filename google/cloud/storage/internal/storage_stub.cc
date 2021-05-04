// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/getenv.h"
#include "absl/memory/memory.h"
#include <google/storage/v1/storage.grpc.pb.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

class StorageStubImpl : public StorageStub {
 public:
  explicit StorageStubImpl(
      std::unique_ptr<google::storage::v1::Storage::StubInterface> impl)
      : impl_(std::move(impl)) {}
  ~StorageStubImpl() override = default;

  std::unique_ptr<ObjectMediaStream> GetObjectMedia(
      std::unique_ptr<grpc::ClientContext> context,
      google::storage::v1::GetObjectMediaRequest const& request) override {
    using ::google::cloud::internal::StreamingReadRpcImpl;
    auto stream = impl_->GetObjectMedia(context.get(), request);
    return absl::make_unique<
        StreamingReadRpcImpl<google::storage::v1::GetObjectMediaResponse>>(
        std::move(context), std::move(stream));
  }

  std::unique_ptr<InsertStream> InsertObjectMedia(
      std::unique_ptr<grpc::ClientContext> context) override {
    using ::google::cloud::internal::StreamingWriteRpcImpl;
    using ResponseType = ::google::storage::v1::Object;
    using RequestType = ::google::storage::v1::InsertObjectRequest;
    auto response = absl::make_unique<ResponseType>();
    auto stream = impl_->InsertObject(context.get(), response.get());
    return absl::make_unique<StreamingWriteRpcImpl<RequestType, ResponseType>>(
        std::move(context), std::move(response), std::move(stream));
  }

  Status DeleteBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteBucketAccessControlRequest const& request)
      override {
    google::protobuf::Empty response;
    auto status =
        impl_->DeleteBucketAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::BucketAccessControl> GetBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetBucketAccessControlRequest const& request)
      override {
    google::storage::v1::BucketAccessControl response;
    auto status = impl_->GetBucketAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::BucketAccessControl> InsertBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertBucketAccessControlRequest const& request)
      override {
    google::storage::v1::BucketAccessControl response;
    auto status =
        impl_->InsertBucketAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ListBucketAccessControlsResponse>
  ListBucketAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListBucketAccessControlsRequest const& request)
      override {
    google::storage::v1::ListBucketAccessControlsResponse response;
    auto status = impl_->ListBucketAccessControls(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::BucketAccessControl> UpdateBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateBucketAccessControlRequest const& request)
      override {
    google::storage::v1::BucketAccessControl response;
    auto status =
        impl_->UpdateBucketAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::BucketAccessControl> PatchBucketAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchBucketAccessControlRequest const& request)
      override {
    google::storage::v1::BucketAccessControl response;
    auto status = impl_->PatchBucketAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteBucket(
      grpc::ClientContext& context,
      google::storage::v1::DeleteBucketRequest const& request) override {
    google::protobuf::Empty response;
    auto status = impl_->DeleteBucket(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::Bucket> GetBucket(
      grpc::ClientContext& context,
      google::storage::v1::GetBucketRequest const& request) override {
    google::storage::v1::Bucket response;
    auto status = impl_->GetBucket(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Bucket> InsertBucket(
      grpc::ClientContext& context,
      google::storage::v1::InsertBucketRequest const& request) override {
    google::storage::v1::Bucket response;
    auto status = impl_->InsertBucket(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ListBucketsResponse> ListBuckets(
      grpc::ClientContext& context,
      google::storage::v1::ListBucketsRequest const& request) override {
    google::storage::v1::ListBucketsResponse response;
    auto status = impl_->ListBuckets(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Bucket> LockBucketRetentionPolicy(
      grpc::ClientContext& context,
      google::storage::v1::LockRetentionPolicyRequest const& request) override {
    google::storage::v1::Bucket response;
    auto status =
        impl_->LockBucketRetentionPolicy(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::iam::v1::Policy> GetBucketIamPolicy(
      grpc::ClientContext& context,
      google::storage::v1::GetIamPolicyRequest const& request) override {
    google::iam::v1::Policy response;
    auto status = impl_->GetBucketIamPolicy(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::iam::v1::Policy> SetBucketIamPolicy(
      grpc::ClientContext& context,
      google::storage::v1::SetIamPolicyRequest const& request) override {
    google::iam::v1::Policy response;
    auto status = impl_->SetBucketIamPolicy(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::iam::v1::TestIamPermissionsResponse>
  TestBucketIamPermissions(
      grpc::ClientContext& context,
      google::storage::v1::TestIamPermissionsRequest const& request) override {
    google::iam::v1::TestIamPermissionsResponse response;
    auto status = impl_->TestBucketIamPermissions(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Bucket> PatchBucket(
      grpc::ClientContext& context,
      google::storage::v1::PatchBucketRequest const& request) override {
    google::storage::v1::Bucket response;
    auto status = impl_->PatchBucket(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Bucket> UpdateBucket(
      grpc::ClientContext& context,
      google::storage::v1::UpdateBucketRequest const& request) override {
    google::storage::v1::Bucket response;
    auto status = impl_->UpdateBucket(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteDefaultObjectAccessControlRequest const&
          request) override {
    google::protobuf::Empty response;
    auto status =
        impl_->DeleteDefaultObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::ObjectAccessControl>
  GetDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetDefaultObjectAccessControlRequest const& request)
      override {
    google::storage::v1::ObjectAccessControl response;
    auto status =
        impl_->GetDefaultObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ObjectAccessControl>
  InsertDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertDefaultObjectAccessControlRequest const&
          request) override {
    google::storage::v1::ObjectAccessControl response;
    auto status =
        impl_->InsertDefaultObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
  ListDefaultObjectAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListDefaultObjectAccessControlsRequest const&
          request) override {
    google::storage::v1::ListObjectAccessControlsResponse response;
    auto status =
        impl_->ListDefaultObjectAccessControls(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ObjectAccessControl>
  PatchDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchDefaultObjectAccessControlRequest const&
          request) override {
    google::storage::v1::ObjectAccessControl response;
    auto status =
        impl_->PatchDefaultObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ObjectAccessControl>
  UpdateDefaultObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateDefaultObjectAccessControlRequest const&
          request) override {
    google::storage::v1::ObjectAccessControl response;
    auto status =
        impl_->UpdateDefaultObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteNotification(
      grpc::ClientContext& context,
      google::storage::v1::DeleteNotificationRequest const& request) override {
    google::protobuf::Empty response;
    auto status = impl_->DeleteNotification(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::Notification> GetNotification(
      grpc::ClientContext& context,
      google::storage::v1::GetNotificationRequest const& request) override {
    google::storage::v1::Notification response;
    auto status = impl_->GetNotification(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Notification> InsertNotification(
      grpc::ClientContext& context,
      google::storage::v1::InsertNotificationRequest const& request) override {
    google::storage::v1::Notification response;
    auto status = impl_->InsertNotification(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ListNotificationsResponse> ListNotifications(
      grpc::ClientContext& context,
      google::storage::v1::ListNotificationsRequest const& request) override {
    google::storage::v1::ListNotificationsResponse response;
    auto status = impl_->ListNotifications(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::DeleteObjectAccessControlRequest const& request)
      override {
    google::protobuf::Empty response;
    auto status =
        impl_->DeleteObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::ObjectAccessControl> GetObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::GetObjectAccessControlRequest const& request)
      override {
    google::storage::v1::ObjectAccessControl response;
    auto status = impl_->GetObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ObjectAccessControl> InsertObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::InsertObjectAccessControlRequest const& request)
      override {
    google::storage::v1::ObjectAccessControl response;
    auto status =
        impl_->InsertObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ListObjectAccessControlsResponse>
  ListObjectAccessControls(
      grpc::ClientContext& context,
      google::storage::v1::ListObjectAccessControlsRequest const& request)
      override {
    google::storage::v1::ListObjectAccessControlsResponse response;
    auto status = impl_->ListObjectAccessControls(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ObjectAccessControl> PatchObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::PatchObjectAccessControlRequest const& request)
      override {
    google::storage::v1::ObjectAccessControl response;
    auto status = impl_->PatchObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ObjectAccessControl> UpdateObjectAccessControl(
      grpc::ClientContext& context,
      google::storage::v1::UpdateObjectAccessControlRequest const& request)
      override {
    google::storage::v1::ObjectAccessControl response;
    auto status =
        impl_->UpdateObjectAccessControl(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Object> ComposeObject(
      grpc::ClientContext& context,
      google::storage::v1::ComposeObjectRequest const& request) override {
    google::storage::v1::Object response;
    auto status = impl_->ComposeObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Object> CopyObject(
      grpc::ClientContext& context,
      google::storage::v1::CopyObjectRequest const& request) override {
    google::storage::v1::Object response;
    auto status = impl_->CopyObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteObject(
      grpc::ClientContext& context,
      google::storage::v1::DeleteObjectRequest const& request) override {
    google::protobuf::Empty response;
    auto status = impl_->DeleteObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::Object> GetObject(
      grpc::ClientContext& context,
      google::storage::v1::GetObjectRequest const& request) override {
    google::storage::v1::Object response;
    auto status = impl_->GetObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ListObjectsResponse> ListObjects(
      grpc::ClientContext& context,
      google::storage::v1::ListObjectsRequest const& request) override {
    google::storage::v1::ListObjectsResponse response;
    auto status = impl_->ListObjects(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::RewriteResponse> RewriteObject(
      grpc::ClientContext& context,
      google::storage::v1::RewriteObjectRequest const& request) override {
    google::storage::v1::RewriteResponse response;
    auto status = impl_->RewriteObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::StartResumableWriteResponse>
  StartResumableWrite(
      grpc::ClientContext& context,
      google::storage::v1::StartResumableWriteRequest const& request) override {
    google::storage::v1::StartResumableWriteResponse response;
    auto status = impl_->StartResumableWrite(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::QueryWriteStatusResponse> QueryWriteStatus(
      grpc::ClientContext& context,
      google::storage::v1::QueryWriteStatusRequest const& request) override {
    google::storage::v1::QueryWriteStatusResponse response;
    auto status = impl_->QueryWriteStatus(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Object> PatchObject(
      grpc::ClientContext& context,
      google::storage::v1::PatchObjectRequest const& request) override {
    google::storage::v1::Object response;
    auto status = impl_->PatchObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::Object> UpdateObject(
      grpc::ClientContext& context,
      google::storage::v1::UpdateObjectRequest const& request) override {
    google::storage::v1::Object response;
    auto status = impl_->UpdateObject(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::ServiceAccount> GetServiceAccount(
      grpc::ClientContext& context,
      google::storage::v1::GetProjectServiceAccountRequest const& request)
      override {
    google::storage::v1::ServiceAccount response;
    auto status = impl_->GetServiceAccount(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  StatusOr<google::storage::v1::CreateHmacKeyResponse> CreateHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::CreateHmacKeyRequest const& request) override {
    google::storage::v1::CreateHmacKeyResponse response;
    auto status = impl_->CreateHmacKey(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

  Status DeleteHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::DeleteHmacKeyRequest const& request) override {
    google::protobuf::Empty response;
    auto status = impl_->DeleteHmacKey(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return Status{};
  }

  StatusOr<google::storage::v1::HmacKeyMetadata> GetHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::GetHmacKeyRequest const& request) override {
    google::storage::v1::HmacKeyMetadata response;
    auto status = impl_->GetHmacKey(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }
  StatusOr<google::storage::v1::ListHmacKeysResponse> ListHmacKeys(
      grpc::ClientContext& context,
      google::storage::v1::ListHmacKeysRequest const& request) override {
    google::storage::v1::ListHmacKeysResponse response;
    auto status = impl_->ListHmacKeys(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }
  StatusOr<google::storage::v1::HmacKeyMetadata> UpdateHmacKey(
      grpc::ClientContext& context,
      google::storage::v1::UpdateHmacKeyRequest const& request) override {
    google::storage::v1::HmacKeyMetadata response;
    auto status = impl_->UpdateHmacKey(&context, request, &response);
    if (!status.ok()) return MakeStatusFromRpcError(status);
    return response;
  }

 private:
  std::unique_ptr<google::storage::v1::Storage::StubInterface> impl_;
};

}  // namespace

std::shared_ptr<StorageStub> MakeDefaultStorageStub(
    std::shared_ptr<grpc::Channel> channel) {
  return std::make_shared<StorageStubImpl>(
      google::storage::v1::Storage::NewStub(std::move(channel)));
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
