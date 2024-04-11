// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_REQUEST_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_REQUEST_PARSER_H

#include "google/cloud/storage/async/object_requests.h"
#include "google/cloud/storage/internal/storage_connection.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/rpc_metadata.h"
#include <google/storage/v2/storage.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::storage::v2::ComposeObjectRequest> ToProto(
    storage::internal::ComposeObjectRequest const& request);

google::storage::v2::DeleteObjectRequest ToProto(
    storage::internal::DeleteObjectRequest const& request);

google::storage::v2::GetObjectRequest ToProto(
    storage::internal::GetObjectMetadataRequest const& request);

StatusOr<google::storage::v2::ReadObjectRequest> ToProto(
    storage::internal::ReadObjectRangeRequest const& request);

StatusOr<google::storage::v2::UpdateObjectRequest> ToProto(
    storage::internal::PatchObjectRequest const& request);
StatusOr<google::storage::v2::UpdateObjectRequest> ToProto(
    storage::internal::UpdateObjectRequest const& request);

StatusOr<google::storage::v2::WriteObjectRequest> ToProto(
    storage::internal::InsertObjectMediaRequest const& request);
storage::internal::QueryResumableUploadResponse FromProto(
    google::storage::v2::WriteObjectResponse const& p, Options const& options,
    google::cloud::RpcMetadata metadata);

google::storage::v2::ListObjectsRequest ToProto(
    storage::internal::ListObjectsRequest const& request);
storage::internal::ListObjectsResponse FromProto(
    google::storage::v2::ListObjectsResponse const& response,
    Options const& options);

StatusOr<google::storage::v2::RewriteObjectRequest> ToProto(
    storage::internal::RewriteObjectRequest const& request);
storage::internal::RewriteObjectResponse FromProto(
    google::storage::v2::RewriteResponse const& response,
    Options const& options);

StatusOr<google::storage::v2::RewriteObjectRequest> ToProto(
    storage::internal::CopyObjectRequest const& request);

StatusOr<google::storage::v2::StartResumableWriteRequest> ToProto(
    storage::internal::ResumableUploadRequest const& request);

google::storage::v2::QueryWriteStatusRequest ToProto(
    storage::internal::QueryResumableUploadRequest const& request);
storage::internal::QueryResumableUploadResponse FromProto(
    google::storage::v2::QueryWriteStatusResponse const& response,
    Options const& options);

google::storage::v2::CancelResumableWriteRequest ToProto(
    storage::internal::DeleteResumableUploadRequest const& request);

Status Finalize(google::storage::v2::WriteObjectRequest& write_request,
                grpc::WriteOptions& options,
                storage::internal::HashFunction& hash_function,
                storage::internal::HashValues = {});

Status Finalize(google::storage::v2::BidiWriteObjectRequest& write_request,
                grpc::WriteOptions& options,
                storage::internal::HashFunction& hash_function,
                storage::internal::HashValues = {});

Status MaybeFinalize(google::storage::v2::WriteObjectRequest& write_request,
                     grpc::WriteOptions& options,
                     storage::internal::InsertObjectMediaRequest const& request,
                     bool chunk_has_more);

Status MaybeFinalize(google::storage::v2::WriteObjectRequest& write_request,
                     grpc::WriteOptions& options,
                     storage::internal::UploadChunkRequest const& request,
                     bool chunk_has_more);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_REQUEST_PARSER_H
