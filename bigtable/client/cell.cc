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

#include "bigtable/client/cell.h"

#include <absl/strings/str_join.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// consolidate concatenates all the chunks and caches the resulting value. It is
// safe to call it twice, after the first call it becomes a no-op.
void Cell::consolidate() const EXCLUSIVE_LOCKS_REQUIRED(mu_) {
  if (chunks_.empty()) {
    return;
  }
  copied_value_ = absl::StrJoin(chunks_, "");
  chunks_.clear();
  value_ = copied_value_;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
