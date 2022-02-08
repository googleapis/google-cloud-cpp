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

#include "google/cloud/storage/internal/grpc_bucket_request_parser.h"
#include "google/cloud/storage/internal/grpc_common_request_params.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

google::storage::v2::GetBucketRequest GrpcBucketRequestParser::ToProto(
    GetBucketMetadataRequest const& request) {
  google::storage::v2::GetBucketRequest result;
  SetCommonParameters(result, request);
  result.set_name("projects/_/buckets/" + request.bucket_name());
  if (request.HasOption<IfMetagenerationMatch>()) {
    result.set_if_metageneration_match(
        request.GetOption<IfMetagenerationMatch>().value());
  }
  if (request.HasOption<IfMetagenerationNotMatch>()) {
    result.set_if_metageneration_not_match(
        request.GetOption<IfMetagenerationNotMatch>().value());
  }
  auto projection = request.GetOption<Projection>().value_or("");
  if (projection == "full") result.mutable_read_mask()->add_paths("*");
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
