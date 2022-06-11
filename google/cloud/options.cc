// Copyright 2021 Google LLC
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

#include "google/cloud/options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <iterator>
#include <set>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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

Options MergeOptions(Options preferred, Options alternatives) {
  if (preferred.m_.empty()) return alternatives;
  preferred.m_.insert(std::make_move_iterator(alternatives.m_.begin()),
                      std::make_move_iterator(alternatives.m_.end()));
  return preferred;
}

namespace {

// The prevailing options for the current operation.  Thread local, so
// additional propagation must be done whenever work for the operation
// is done in another thread.
Options& ThreadLocalOptions() {
  thread_local Options current_options;
  return current_options;
}

}  // namespace

Options const& CurrentOptions() { return ThreadLocalOptions(); }

OptionsSpan::OptionsSpan(Options opts) : opts_(std::move(opts)) {
  using std::swap;
  swap(opts_, ThreadLocalOptions());
}

OptionsSpan::~OptionsSpan() { ThreadLocalOptions() = std::move(opts_); }

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
