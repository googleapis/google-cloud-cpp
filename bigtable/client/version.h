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

#ifndef BIGTABLE_CLIENT_VERSION_H_
#define BIGTABLE_CLIENT_VERSION_H_

#if __cplusplus < 201103L
#error "Bigtable C++ Client requires C++11"
#endif  // __cplusplus < 201103L

#define BIGTABLE_CLIENT_VERSION_MAJOR 0
#define BIGTABLE_CLIENT_VERSION_MINOR 1

#define BIGTABLE_CLIENT_VCONCAT(Ma, Mi) v##Ma
#define BIGTABLE_CLIENT_VEVAL(Ma, Mi) BIGTABLE_CLIENT_VCONCAT(Ma, Mi)
#define BIGTABLE_CLIENT_NS                             \
  BIGTABLE_CLIENT_VEVAL(BIGTABLE_CLIENT_VERSION_MAJOR, \
                        BIGTABLE_CLIENT_VERSION_MINOR)

namespace bigtable {
int constexpr version_major() { return BIGTABLE_CLIENT_VERSION_MAJOR; }
int constexpr version_minor() { return BIGTABLE_CLIENT_VERSION_MINOR; }
/// A single integer representing the Major/Minor version
int constexpr version() { return 100 * version_major() + version_minor(); }
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_VERSION_H_
