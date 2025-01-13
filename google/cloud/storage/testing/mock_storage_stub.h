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

#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage {
namespace testing {

class MockStorageStub : public storage_internal::StorageStub {
 public:
  MOCK_METHOD(Status, DeleteBucket,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::DeleteBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, GetBucket,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::GetBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, CreateBucket,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::CreateBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::ListBucketsResponse>, ListBuckets,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::ListBucketsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, LockBucketRetentionPolicy,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::LockBucketRetentionPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&, Options const&,
               google::iam::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&, Options const&,
               google::iam::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&, Options const&,
               google::iam::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Bucket>, UpdateBucket,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::UpdateBucketRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, ComposeObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::ComposeObjectRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::DeleteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, RestoreObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::RestoreObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::CancelResumableWriteResponse>,
              CancelResumableWrite,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::CancelResumableWriteRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, GetObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::GetObjectRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<google::cloud::internal::StreamingReadRpc<
                  google::storage::v2::ReadObjectResponse>>,
              ReadObject,
              (std::shared_ptr<grpc::ClientContext>, Options const&,
               google::storage::v2::ReadObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, UpdateObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::UpdateObjectRequest const&),
              (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
                   google::storage::v2::BidiReadObjectRequest,
                   google::storage::v2::BidiReadObjectResponse>>),
              AsyncBidiReadObject,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions),
              (override));

  MOCK_METHOD((std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
                   google::storage::v2::WriteObjectRequest,
                   google::storage::v2::WriteObjectResponse>>),
              WriteObject,
              (std::shared_ptr<grpc::ClientContext>, Options const&),
              (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
                   google::storage::v2::BidiWriteObjectRequest,
                   google::storage::v2::BidiWriteObjectResponse>>),
              AsyncBidiWriteObject,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions),
              (override));

  MOCK_METHOD(StatusOr<google::storage::v2::ListObjectsResponse>, ListObjects,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::ListObjectsRequest const&));
  MOCK_METHOD(StatusOr<google::storage::v2::RewriteResponse>, RewriteObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::RewriteObjectRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::StartResumableWriteResponse>,
              StartResumableWrite,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::StartResumableWriteRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::QueryWriteStatusResponse>,
              QueryWriteStatus,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::QueryWriteStatusRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::storage::v2::Object>, MoveObject,
              (grpc::ClientContext&, Options const&,
               google::storage::v2::MoveObjectRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::storage::v2::Object>>, AsyncComposeObject,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::storage::v2::ComposeObjectRequest const&),
              (override));
  MOCK_METHOD(future<Status>, AsyncDeleteObject,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::storage::v2::DeleteObjectRequest const&),
              (override));
  MOCK_METHOD(std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
                  google::storage::v2::ReadObjectResponse>>,
              AsyncReadObject,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::storage::v2::ReadObjectRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::storage::v2::RewriteResponse>>,
              AsyncRewriteObject,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::storage::v2::RewriteObjectRequest const&),
              (override));
  using AsyncWriteObjectReturnType =
      std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
          google::storage::v2::WriteObjectRequest,
          google::storage::v2::WriteObjectResponse>>;
  MOCK_METHOD(AsyncWriteObjectReturnType, AsyncWriteObject,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions),
              (override));
  MOCK_METHOD(
      future<StatusOr<google::storage::v2::StartResumableWriteResponse>>,
      AsyncStartResumableWrite,
      (google::cloud::CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       google::storage::v2::StartResumableWriteRequest const&),
      (override));
  MOCK_METHOD(future<StatusOr<google::storage::v2::QueryWriteStatusResponse>>,
              AsyncQueryWriteStatus,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::storage::v2::QueryWriteStatusRequest const&),
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
  MOCK_METHOD(google::cloud::RpcMetadata, GetRequestMetadata, (),
              (const, override));
};

class MockObjectMediaStream : public google::cloud::internal::StreamingReadRpc<
                                  google::storage::v2::ReadObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::variant<Status, google::storage::v2::ReadObjectResponse>),
              Read, (), (override));
  MOCK_METHOD(google::cloud::RpcMetadata, GetRequestMetadata, (),
              (const, override));
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
  MOCK_METHOD(google::cloud::RpcMetadata, GetRequestMetadata, (),
              (const, override));
};

using MockAsyncObjectMediaStream =
    google::cloud::testing_util::MockAsyncStreamingReadRpc<
        google::storage::v2::ReadObjectResponse>;

using MockAsyncBidiReadObjectStream =
    google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
        google::storage::v2::BidiReadObjectRequest,
        google::storage::v2::BidiReadObjectResponse>;

using MockAsyncBidiWriteObjectStream =
    google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
        google::storage::v2::BidiWriteObjectRequest,
        google::storage::v2::BidiWriteObjectResponse>;

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_MOCK_STORAGE_STUB_H
