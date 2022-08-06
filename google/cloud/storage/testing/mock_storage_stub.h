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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H

#include "google/cloud/storage/internal/storage_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockStorageStub : public storage_internal::StorageStub {
 public:
  MOCK_METHOD(Status, DeleteBucket,
              (grpc::ClientContext&,
               google::storage::v2::DeleteBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, GetBucket,
              (grpc::ClientContext&,
               google::storage::v2::GetBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, CreateBucket,
              (grpc::ClientContext&,
               google::storage::v2::CreateBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::ListBucketsResponse>, ListBuckets,
              (grpc::ClientContext&,
               google::storage::v2::ListBucketsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, LockBucketRetentionPolicy,
              (grpc::ClientContext&,
               google::storage::v2::LockBucketRetentionPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&,
               google::iam::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&,
               google::iam::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&,
               google::iam::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, UpdateBucket,
              (grpc::ClientContext&,
               google::storage::v2::UpdateBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Notification>, GetNotification,
              (grpc::ClientContext&,
               google::storage::v2::GetNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Notification>, CreateNotification,
              (grpc::ClientContext&,
               google::storage::v2::CreateNotificationRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, ComposeObject,
              (grpc::ClientContext&,
               google::storage::v2::ComposeObjectRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteObject,
              (grpc::ClientContext&,
               google::storage::v2::DeleteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::CancelResumableWriteResponse>,
              CancelResumableWrite,
              (grpc::ClientContext&,
               google::storage::v2::CancelResumableWriteRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, GetObject,
              (grpc::ClientContext&,
               google::storage::v2::GetObjectRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::storage::v2::ReadObjectResponse>>,
              ReadObject,
              (std::unique_ptr<grpc::ClientContext>,
               google::storage::v2::ReadObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, UpdateObject,
              (grpc::ClientContext&,
               google::storage::v2::UpdateObjectRequest const&),
              (override));
  MOCK_METHOD((std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
                   google::storage::v2::WriteObjectRequest,
                   google::storage::v2::WriteObjectResponse>>),
              WriteObject, (std::unique_ptr<grpc::ClientContext>), (override));
  MOCK_METHOD(StatusOr<google::storage::v2::ListObjectsResponse>, ListObjects,
              (grpc::ClientContext&,
               google::storage::v2::ListObjectsRequest const&));
  MOCK_METHOD(StatusOr<google::storage::v2::RewriteResponse>, RewriteObject,
              (grpc::ClientContext&,
               google::storage::v2::RewriteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::StartResumableWriteResponse>,
              StartResumableWrite,
              (grpc::ClientContext&,
               google::storage::v2::StartResumableWriteRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::QueryWriteStatusResponse>,
              QueryWriteStatus,
              (grpc::ClientContext&,
               google::storage::v2::QueryWriteStatusRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::ServiceAccount>, GetServiceAccount,
              (grpc::ClientContext&,
               google::storage::v2::GetServiceAccountRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::CreateHmacKeyResponse>,
              CreateHmacKey,
              (grpc::ClientContext&,
               google::storage::v2::CreateHmacKeyRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteHmacKey,
              (grpc::ClientContext&,
               google::storage::v2::DeleteHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::HmacKeyMetadata>, GetHmacKey,
              (grpc::ClientContext&,
               google::storage::v2::GetHmacKeyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::ListHmacKeysResponse>, ListHmacKeys,
              (grpc::ClientContext&,
               google::storage::v2::ListHmacKeysRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::HmacKeyMetadata>, UpdateHmacKey,
              (grpc::ClientContext&,
               google::storage::v2::UpdateHmacKeyRequest const&),
              (override));
  MOCK_METHOD(future<Status>, AsyncDeleteObject,
              (google::cloud::CompletionQueue&,
               std::unique_ptr<grpc::ClientContext>,
               google::storage::v2::DeleteObjectRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
                  google::storage::v2::ReadObjectResponse>>,
              AsyncReadObject,
              (google::cloud::CompletionQueue const&,
               std::unique_ptr<grpc::ClientContext>,
               google::storage::v2::ReadObjectRequest const&),
              (override));
  using AsyncWriteObjectReturnType =
      std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
          google::storage::v2::WriteObjectRequest,
          google::storage::v2::WriteObjectResponse>>;
  MOCK_METHOD(AsyncWriteObjectReturnType, AsyncWriteObject,
              (google::cloud::CompletionQueue const&,
               std::unique_ptr<grpc::ClientContext>),
              (override));
};

class MockInsertStream : public google::cloud::internal::StreamingWriteRpc<
                             google::storage::v2::WriteObjectRequest,
                             google::storage::v2::WriteObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(bool, Write,
              (google::storage::v2::WriteObjectRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::WriteObjectResponse>, Close, (),
              (override));
  MOCK_METHOD(google::cloud::internal::StreamingRpcMetadata, GetRequestMetadata,
              (), (const, override));
};

class MockObjectMediaStream : public google::cloud::internal::StreamingReadRpc<
                                  google::storage::v2::ReadObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::variant<Status, google::storage::v2::ReadObjectResponse>),
              Read, (), (override));
  MOCK_METHOD(google::cloud::internal::StreamingRpcMetadata, GetRequestMetadata,
              (), (const, override));
};

class MockAsyncInsertStream
    : public google::cloud::internal::AsyncStreamingWriteRpc<
          google::storage::v2::WriteObjectRequest,
          google::storage::v2::WriteObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<bool>, Write,
              (google::storage::v2::WriteObjectRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(future<StatusOr<google::storage::v2::WriteObjectResponse>>,
              Finish, (), (override));
  MOCK_METHOD(google::cloud::internal::StreamingRpcMetadata, GetRequestMetadata,
              (), (const, override));
};

class MockAsyncObjectMediaStream
    : public google::cloud::internal::AsyncStreamingReadRpc<
          google::storage::v2::ReadObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<google::storage::v2::ReadObjectResponse>>,
              Read, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
  MOCK_METHOD(google::cloud::internal::StreamingRpcMetadata, GetRequestMetadata,
              (), (const, override));
};

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
