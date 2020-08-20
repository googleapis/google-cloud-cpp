// Copyright 2020 Google LLC
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

#include "google/cloud/bigtable/internal/api_client_header.h"
#include "google/cloud/internal/compiler_info.h"
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

std::string ApiClientHeader() {
  return "gl-cpp/" + google::cloud::internal::CompilerId() + "-" +
         google::cloud::internal::CompilerVersion() + "-" +
         google::cloud::internal::CompilerFeatures() + "-" +
         google::cloud::internal::LanguageVersion() + " gccl/" +
         version_string();
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
