// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SYNTHETIC_SELF_LINK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SYNTHETIC_SELF_LINK_H

#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string SyntheticSelfLinkRoot(Options const& options);
std::string SyntheticSelfLinkDownloadRoot(Options const& options);
std::string SyntheticSelfLinkBucket(Options const& options,
                                    std::string const& bucket_name);
std::string SyntheticSelfLinkObject(Options const& options,
                                    std::string const& bucket_name,
                                    std::string const& object_name);
std::string SyntheticSelfLinkDownload(Options const& options,
                                      std::string const& bucket_name,
                                      std::string const& object_name,
                                      std::int64_t generation);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SYNTHETIC_SELF_LINK_H
