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

#include "google/cloud/version.h"
#include "google/cloud/internal/build_info.h"
#include <sstream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
std::string version_string() {
  static std::string const kVersion = [] {
    std::ostringstream os;
    os << "v" << version_major() << "." << version_minor() << "."
       << version_patch();
    auto metadata = google::cloud::internal::build_metadata();
    if (!metadata.empty()) {
      os << "+" << metadata;
    }
    return os.str();
  }();
  return kVersion;
}
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
