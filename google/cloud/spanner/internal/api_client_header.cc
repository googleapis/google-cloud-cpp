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

#include "google/cloud/spanner/internal/api_client_header.h"
#include "google/cloud/spanner/internal/compiler_info.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

std::string ApiClientHeader() {
  return "gl-cpp/" + google::cloud::spanner::internal::CompilerId() + "-" +
         google::cloud::spanner::internal::CompilerVersion() + "-" +
         google::cloud::spanner::internal::CompilerFeatures() + "-" +
         google::cloud::spanner::internal::LanguageVersion() + " gccl/" +
         VersionString();
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
