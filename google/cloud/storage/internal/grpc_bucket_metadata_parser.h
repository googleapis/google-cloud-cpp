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
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::storage::v2::Bucket ToProto(storage::BucketMetadata const& rhs);
storage::BucketMetadata FromProto(google::storage::v2::Bucket const& rhs);

google::storage::v2::Bucket::Billing ToProto(storage::BucketBilling const& rhs);
storage::BucketBilling FromProto(
    google::storage::v2::Bucket::Billing const& rhs);

google::storage::v2::Bucket::Cors ToProto(storage::CorsEntry const& rhs);
storage::CorsEntry FromProto(google::storage::v2::Bucket::Cors const& rhs);

google::storage::v2::Bucket::Encryption ToProto(
    storage::BucketEncryption const& rhs);
storage::BucketEncryption FromProto(
    google::storage::v2::Bucket::Encryption const& rhs);

google::storage::v2::Bucket::IamConfig ToProto(
    storage::BucketIamConfiguration const& rhs);
storage::BucketIamConfiguration FromProto(
    google::storage::v2::Bucket::IamConfig const& rhs);

google::storage::v2::Bucket::Lifecycle::Rule::Action ToProto(
    storage::LifecycleRuleAction rhs);
storage::LifecycleRuleAction FromProto(
    google::storage::v2::Bucket::Lifecycle::Rule::Action rhs);

google::storage::v2::Bucket::Lifecycle::Rule::Condition ToProto(
    storage::LifecycleRuleCondition rhs);
storage::LifecycleRuleCondition FromProto(
    google::storage::v2::Bucket::Lifecycle::Rule::Condition rhs);

google::storage::v2::Bucket::Lifecycle::Rule ToProto(
    storage::LifecycleRule const& rhs);
storage::LifecycleRule FromProto(
    google::storage::v2::Bucket::Lifecycle::Rule rhs);

google::storage::v2::Bucket::Lifecycle ToProto(
    storage::BucketLifecycle const& rhs);
storage::BucketLifecycle FromProto(google::storage::v2::Bucket::Lifecycle rhs);

google::storage::v2::Bucket::Logging ToProto(storage::BucketLogging const& rhs);
storage::BucketLogging FromProto(
    google::storage::v2::Bucket::Logging const& rhs);

google::storage::v2::Bucket::RetentionPolicy ToProto(
    storage::BucketRetentionPolicy const& rhs);
storage::BucketRetentionPolicy FromProto(
    google::storage::v2::Bucket::RetentionPolicy const& rhs);

google::storage::v2::Bucket::Versioning ToProto(
    storage::BucketVersioning const& rhs);
storage::BucketVersioning FromProto(
    google::storage::v2::Bucket::Versioning const& rhs);

google::storage::v2::Bucket::Website ToProto(storage::BucketWebsite rhs);
storage::BucketWebsite FromProto(google::storage::v2::Bucket::Website rhs);

google::storage::v2::Bucket::CustomPlacementConfig ToProto(
    storage::BucketCustomPlacementConfig rhs);
storage::BucketCustomPlacementConfig FromProto(
    google::storage::v2::Bucket::CustomPlacementConfig rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_METADATA_PARSER_H
