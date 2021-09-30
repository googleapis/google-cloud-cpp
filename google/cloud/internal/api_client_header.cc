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

#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/compiler_info.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::string ApiClientVersion(std::string const& build_identifier) {
  auto client_library_version = version_string();
  if (!client_library_version.empty() && client_library_version[0] == 'v') {
    // Remove the leading 'v'. Without it, version_string() is a valid
    // SemVer string: "<major>.<minor>.<patch>[-<prerelease>][+<build>]".
    client_library_version.erase(0, 1);
  }
  if (!build_identifier.empty()) {
    auto pos = client_library_version.find('+');
    client_library_version.append(1, pos == std::string::npos ? '+' : '.');
    client_library_version.append(build_identifier);
  }
  return client_library_version;
}

std::string ApiClientHeader(std::string const& build_identifier) {
  return "gl-cpp/" + google::cloud::internal::CompilerId() + "-" +
         google::cloud::internal::CompilerVersion() + "-" +
         google::cloud::internal::CompilerFeatures() + "-" +
         google::cloud::internal::LanguageVersion() + " gccl/" +
         ApiClientVersion(build_identifier);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
