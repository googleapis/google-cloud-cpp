// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_UTILS_VERSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_UTILS_VERSION_H

#include "google/cloud/version.h"

namespace google {
namespace cloud {
/**
 * Contains all the Cloud C++ gRPC Utilities APIs.
 */
namespace grpc_utils {
/// Versioned inline namespace that users should generally avoid spelling.
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
using ::google::cloud::version;
using ::google::cloud::version_major;
using ::google::cloud::version_minor;
using ::google::cloud::version_patch;
using ::google::cloud::version_string;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace grpc_utils
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_GRPC_UTILS_VERSION_H
