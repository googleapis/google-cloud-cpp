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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_METADATA_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_METADATA_PARSER_H

#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"
#include <google/storage/v2/storage.pb.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

struct GrpcBucketMetadataParser {
  static google::storage::v2::Bucket ToProto(BucketMetadata const& rhs);
  static BucketMetadata FromProto(google::storage::v2::Bucket const& rhs);

  static google::storage::v2::Bucket::Billing ToProto(BucketBilling const& rhs);
  static BucketBilling FromProto(
      google::storage::v2::Bucket::Billing const& rhs);

  static google::storage::v2::Bucket::Cors ToProto(CorsEntry const& rhs);
  static CorsEntry FromProto(google::storage::v2::Bucket::Cors const& rhs);

  static google::storage::v2::Bucket::Encryption ToProto(
      BucketEncryption const& rhs);
  static BucketEncryption FromProto(
      google::storage::v2::Bucket::Encryption const& rhs);

  static google::storage::v2::Bucket::IamConfig ToProto(
      BucketIamConfiguration const& rhs);
  static BucketIamConfiguration FromProto(
      google::storage::v2::Bucket::IamConfig const& rhs);

  static google::storage::v2::Bucket::Lifecycle::Rule::Action ToProto(
      LifecycleRuleAction rhs);
  static LifecycleRuleAction FromProto(
      google::storage::v2::Bucket::Lifecycle::Rule::Action rhs);

  static google::storage::v2::Bucket::Lifecycle::Rule::Condition ToProto(
      LifecycleRuleCondition rhs);
  static LifecycleRuleCondition FromProto(
      google::storage::v2::Bucket::Lifecycle::Rule::Condition rhs);

  static google::storage::v2::Bucket::Lifecycle::Rule ToProto(
      LifecycleRule rhs);
  static LifecycleRule FromProto(
      google::storage::v2::Bucket::Lifecycle::Rule rhs);

  static google::storage::v2::Bucket::Lifecycle ToProto(BucketLifecycle rhs);
  static BucketLifecycle FromProto(google::storage::v2::Bucket::Lifecycle rhs);

  static google::storage::v2::Bucket::Logging ToProto(BucketLogging const& rhs);
  static BucketLogging FromProto(
      google::storage::v2::Bucket::Logging const& rhs);

  static google::storage::v2::Bucket::RetentionPolicy ToProto(
      BucketRetentionPolicy const& rhs);
  static BucketRetentionPolicy FromProto(
      google::storage::v2::Bucket::RetentionPolicy const& rhs);

  static google::storage::v2::Bucket::Versioning ToProto(
      BucketVersioning const& rhs);
  static BucketVersioning FromProto(
      google::storage::v2::Bucket::Versioning const& rhs);

  static google::storage::v2::Bucket::Website ToProto(BucketWebsite rhs);
  static BucketWebsite FromProto(google::storage::v2::Bucket::Website rhs);

  static google::storage::v2::Bucket::CustomPlacementConfig ToProto(
      BucketCustomPlacementConfig rhs);
  static BucketCustomPlacementConfig FromProto(
      google::storage::v2::Bucket::CustomPlacementConfig rhs);
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_METADATA_PARSER_H
