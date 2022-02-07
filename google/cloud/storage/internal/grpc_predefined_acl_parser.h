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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_PREDEFINED_ACL_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_PREDEFINED_ACL_PARSER_H

#include "google/cloud/storage/version.h"
#include "google/cloud/storage/well_known_parameters.h"
#include <google/storage/v2/storage.pb.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/// Convert JSON `predefined*Acl` strings into the proto enum values
struct GrpcPredefinedAclParser {
  static google::storage::v2::PredefinedObjectAcl ToProtoObject(
      PredefinedAcl const& acl);
  static google::storage::v2::PredefinedObjectAcl ToProtoObject(
      DestinationPredefinedAcl const& acl);
  static google::storage::v2::PredefinedObjectAcl ToProtoObject(
      PredefinedDefaultObjectAcl const& acl);

  static google::storage::v2::PredefinedBucketAcl ToProtoBucket(
      PredefinedAcl const& acl);
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_PREDEFINED_ACL_PARSER_H
