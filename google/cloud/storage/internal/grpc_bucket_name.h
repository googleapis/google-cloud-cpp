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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_NAME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_NAME_H

#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 *  Convert from bucket ids to bucket names for the gRPC transport.
 *
 * In REST the [bucket] name and bucket id properties have identical values. In
 * gRPC the bucket names are prefixed with `projects/_/buckets/`. This function
 * adds that prefix.
 *
 *  [bucket]: https://cloud.google.com/storage/docs/json_api/v1/buckets
 */
std::string GrpcBucketIdToName(std::string const& id);

/**
 *  Convert from bucket names to bucket ids for the gRPC transport.
 *
 * In REST the [bucket] name and bucket id properties have identical values. In
 * gRPC the bucket names are prefixed with `projects/_/buckets/`. This function
 * removes that prefix.
 *
 *  [bucket]: https://cloud.google.com/storage/docs/json_api/v1/buckets
 */
std::string GrpcBucketNameToId(std::string const& name);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_BUCKET_NAME_H
