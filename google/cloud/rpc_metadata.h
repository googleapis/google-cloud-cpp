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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RPC_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RPC_METADATA_H

#include "google/cloud/version.h"
#include <map>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * RPC request metadata.
 *
 * Once a RPC completes the underlying transport (gRPC or HTTP) may make some
 * metadata attributes about the request available. For the moment we just
 * capture headers, trailers, and tunnel some low-level socket information as
 * fake headers.
 *
 * @note When available to applications this information is intended only for
 *   debugging and troubleshooting.  There is no guarantee that the header names
 *   will always be available, or remain stable.
 */
struct RpcMetadata {
  std::multimap<std::string, std::string> headers;
  std::multimap<std::string, std::string> trailers;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RPC_METADATA_H
