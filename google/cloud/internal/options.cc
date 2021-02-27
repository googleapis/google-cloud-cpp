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

#include "google/cloud/internal/options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
#include <set>
#include <unordered_map>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

void CheckExpectedOptionsImpl(std::set<std::type_index> const& expected,
                              Options const& opts, char const* const caller) {
  for (auto const& p : opts.m_) {
    if (!Contains(expected, p.first)) {
      GCP_LOG(WARNING) << caller << ": Unexpected option (mangled name): "
                       << p.first.name();
    }
  }
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
