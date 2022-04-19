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

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include <google/storage/v2/storage.pb.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/// Convert JSON requests to gRPC requests and gRPC responses to JSON responses
struct GrpcObjectRequestParser {
  static StatusOr<google::storage::v2::ComposeObjectRequest> ToProto(
      ComposeObjectRequest const& request);

  static google::storage::v2::DeleteObjectRequest ToProto(
      DeleteObjectRequest const& request);

  static google::storage::v2::GetObjectRequest ToProto(
      GetObjectMetadataRequest const& request);

  static StatusOr<google::storage::v2::ReadObjectRequest> ToProto(
      ReadObjectRangeRequest const& request);

  static StatusOr<google::storage::v2::UpdateObjectRequest> ToProto(
      PatchObjectRequest const& request);
  static StatusOr<google::storage::v2::UpdateObjectRequest> ToProto(
      UpdateObjectRequest const& request);

  static StatusOr<google::storage::v2::WriteObjectRequest> ToProto(
      InsertObjectMediaRequest const& request);
  static QueryResumableUploadResponse FromProto(
      google::storage::v2::WriteObjectResponse const& p,
      Options const& options);

  static google::storage::v2::ListObjectsRequest ToProto(
      ListObjectsRequest const& request);
  static ListObjectsResponse FromProto(
      google::storage::v2::ListObjectsResponse const& response,
      Options const& options);

  static StatusOr<google::storage::v2::RewriteObjectRequest> ToProto(
      RewriteObjectRequest const& request);
  static RewriteObjectResponse FromProto(
      google::storage::v2::RewriteResponse const& response,
      Options const& options);

  static StatusOr<google::storage::v2::RewriteObjectRequest> ToProto(
      CopyObjectRequest const& request);

  static StatusOr<google::storage::v2::StartResumableWriteRequest> ToProto(
      ResumableUploadRequest const& request);

  static google::storage::v2::QueryWriteStatusRequest ToProto(
      QueryResumableUploadRequest const& request);
  static QueryResumableUploadResponse FromProto(
      google::storage::v2::QueryWriteStatusResponse const& response,
      Options const& options);
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_REQUEST_PARSER_H
