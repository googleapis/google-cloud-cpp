// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_REQUEST_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_REQUEST_PARSER_H

#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/version.h"
#include <google/storage/v2/storage.pb.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Convert JSON requests to gRPC requests and gRPC responses to JSON responses
google::storage::v2::DeleteBucketRequest ToProto(
    storage::internal::DeleteBucketRequest const& request);

google::storage::v2::GetBucketRequest ToProto(
    storage::internal::GetBucketMetadataRequest const& request);

google::storage::v2::CreateBucketRequest ToProto(
    storage::internal::CreateBucketRequest const& request);

google::storage::v2::ListBucketsRequest ToProto(
    storage::internal::ListBucketsRequest const& request);
storage::internal::ListBucketsResponse FromProto(
    google::storage::v2::ListBucketsResponse const& response);

google::storage::v2::LockBucketRetentionPolicyRequest ToProto(
    storage::internal::LockBucketRetentionPolicyRequest const& request);

google::iam::v1::GetIamPolicyRequest ToProto(
    storage::internal::GetBucketIamPolicyRequest const& request);
storage::NativeIamBinding FromProto(google::iam::v1::Binding const& b);
storage::NativeIamPolicy FromProto(google::iam::v1::Policy const& response);

google::iam::v1::SetIamPolicyRequest ToProto(
    storage::internal::SetNativeBucketIamPolicyRequest const& request);

google::iam::v1::TestIamPermissionsRequest ToProto(
    storage::internal::TestBucketIamPermissionsRequest const& request);
storage::internal::TestBucketIamPermissionsResponse FromProto(
    google::iam::v1::TestIamPermissionsResponse const& response);

StatusOr<google::storage::v2::UpdateBucketRequest> ToProto(
    storage::internal::PatchBucketRequest const& request);
google::storage::v2::UpdateBucketRequest ToProto(
    storage::internal::UpdateBucketRequest const& request);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_REQUEST_PARSER_H
