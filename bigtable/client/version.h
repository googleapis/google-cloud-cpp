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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_VERSION_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_VERSION_H_

#include <string>

#include "bigtable/client/internal/port_platform.h"
#include "bigtable/client/version_info.h"

#define BIGTABLE_CLIENT_VCONCAT(Ma, Mi) v##Ma
#define BIGTABLE_CLIENT_VEVAL(Ma, Mi) BIGTABLE_CLIENT_VCONCAT(Ma, Mi)
#define BIGTABLE_CLIENT_NS                             \
  BIGTABLE_CLIENT_VEVAL(BIGTABLE_CLIENT_VERSION_MAJOR, \
                        BIGTABLE_CLIENT_VERSION_MINOR)

namespace bigtable {
int constexpr version_major() { return BIGTABLE_CLIENT_VERSION_MAJOR; }
int constexpr version_minor() { return BIGTABLE_CLIENT_VERSION_MINOR; }
int constexpr version_patch() { return BIGTABLE_CLIENT_VERSION_PATCH; }
/// A single integer representing the Major/Minor version
int constexpr version() {
  return 100 * (100 * version_major() + version_minor()) + version_patch();
}
std::string version_string();
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_VERSION_H_
