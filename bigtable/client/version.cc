// Copyright 2017 Google Inc.
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

#include "bigtable/client/version.h"
#include "bigtable/client/build_info.h"
#include <sstream>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::string version_string() {
  auto create_version = []() -> std::string {
    std::ostringstream os;
    os << "v" << version_major() << "." << version_minor() << "."
       << version_patch() << "+" << gitrev;
    return os.str();
  };
  static std::string const version = create_version();
  return version;
}
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
