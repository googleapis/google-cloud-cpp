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

#include "google/cloud/storage/internal/grpc/synthetic_self_link.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/url_encode.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::UrlEncode;
using ::google::cloud::storage::RestEndpointOption;
using ::google::cloud::storage::internal::TargetApiVersionOption;

std::string SelfLinkEndpoint(Options const& options) {
  auto endpoint = options.get<RestEndpointOption>();
  if (!endpoint.empty() && endpoint != "https://storage.googleapis.com") {
    return endpoint;
  }
  return "https://www.googleapis.com";
}

std::string SelfLinkPath(Options const& options) {
  if (!options.has<TargetApiVersionOption>()) return "/storage/v1";
  return absl::StrCat("/storage/", options.get<TargetApiVersionOption>());
}

}  // namespace

std::string SyntheticSelfLinkRoot(Options const& options) {
  return absl::StrCat(SelfLinkEndpoint(options), SelfLinkPath(options));
}

std::string SyntheticSelfLinkDownloadRoot(Options const& options) {
  if (options.has<RestEndpointOption>()) {
    return absl::StrCat(options.get<RestEndpointOption>(), "/download",
                        SelfLinkPath(options));
  }
  return absl::StrCat("https://storage.googleapis.com/download",
                      SelfLinkPath(options));
}

std::string SyntheticSelfLinkBucket(Options const& options,
                                    std::string const& bucket_name) {
  return absl::StrCat(SyntheticSelfLinkRoot(options), "/b/", bucket_name);
}

std::string SyntheticSelfLinkObject(Options const& options,
                                    std::string const& bucket_name,
                                    std::string const& object_name) {
  return absl::StrCat(SyntheticSelfLinkRoot(options), "/b/", bucket_name, "/o/",
                      UrlEncode(object_name));
}

std::string SyntheticSelfLinkDownload(Options const& options,
                                      std::string const& bucket_name,
                                      std::string const& object_name,
                                      std::int64_t generation) {
  return absl::StrCat(SyntheticSelfLinkDownloadRoot(options), "/b/",
                      bucket_name, "/o/", UrlEncode(object_name),
                      "?generation=", generation, "&alt=media");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
