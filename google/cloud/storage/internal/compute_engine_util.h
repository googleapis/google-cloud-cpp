// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPUTE_ENGINE_UTIL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPUTE_ENGINE_UTIL_H

#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

/**
 * Returns the env var used to override the check for if we're running on GCE.
 *
 * This environment variable is used for testing to override the return value
 * for the function that checks whether we're running on a GCE VM. Some CI
 * testing services sometimes run on GCE VMs, and we don't want to accidentally
 * try to use their service account credentials during our tests.
 *
 * If set to "1", this will force `RunningOnComputeEngineVm` to return true. If
 * set to anything else, it will return false. If unset, the function will
 * actually check whether we're running on a GCE VM.
 */
inline char const* GceCheckOverrideEnvVar() {
  static constexpr char kEnvVarName[] = "GOOGLE_RUNNING_ON_GCE_CHECK_OVERRIDE";
  return kEnvVarName;
}

/// Returns the hostname for a GCE instance's metadata server.
std::string GceMetadataHostname();

inline char const* GceMetadataHostnameEnvVar() {
  static constexpr char kEnvVarName[] = "GCE_METADATA_ROOT";
  return kEnvVarName;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPUTE_ENGINE_UTIL_H
