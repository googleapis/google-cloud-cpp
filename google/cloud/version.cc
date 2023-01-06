// Copyright 2019 Google LLC
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

#include "google/cloud/version.h"
#include "google/cloud/internal/build_info.h"
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string version_string() {
  static auto const* const kVersion = new auto([] {
    std::ostringstream os;
    os << "v" << version_major() << "." << version_minor() << "."
       << version_patch();
    char const* pre_release = version_pre_release();
    if (*pre_release != '\0') {
      os << "-" << pre_release;
    }
    auto metadata = google::cloud::internal::build_metadata();
    if (!metadata.empty()) {
      os << "+" << metadata;
    }
    return os.str();
  }());
  return *kVersion;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
