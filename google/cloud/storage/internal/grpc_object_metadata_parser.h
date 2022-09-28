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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_METADATA_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_METADATA_PARSER_H

#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#include <google/storage/v2/storage.pb.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<google::storage::v2::CustomerEncryption> ToProto(
    storage::CustomerEncryption rhs);
storage::CustomerEncryption FromProto(
    google::storage::v2::CustomerEncryption rhs);

std::string Crc32cFromProto(std::uint32_t);
StatusOr<std::uint32_t> Crc32cToProto(std::string const&);
std::string MD5FromProto(std::string const&);
StatusOr<std::string> MD5ToProto(std::string const&);
std::string ComputeMD5Hash(std::string const& payload);

storage::ObjectMetadata FromProto(google::storage::v2::Object object,
                                  Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_OBJECT_METADATA_PARSER_H
