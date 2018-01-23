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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_BUILD_INFO_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_BUILD_INFO_H_

#include "bigtable/client/version.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// The git revision at compile time
extern char const gitrev[];

/// The compiler version
extern char const compiler[];

/// The compiler flags
extern char const compiler_flags[];

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_BUILD_INFO_H_
