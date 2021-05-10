// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/options.h"

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

// This empty .cc file eliminated false positive errors from clang-tidy in our
// presubmit CI. Not exactly sure why, but I was able to reproduce the error
// locally with:
//   $ KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH=main
//         KOKORO_JOB_TYPE=PRESUBMIT_GITHUB
//         ./ci/kokoro/docker/build.sh clang-tidy

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
