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

#include "google/cloud/bigtable/idempotent_mutation_policy.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<IdempotentMutationPolicy> DefaultIdempotentMutationPolicy() {
  return std::unique_ptr<IdempotentMutationPolicy>(
      new SafeIdempotentMutationPolicy);
}

std::unique_ptr<IdempotentMutationPolicy> SafeIdempotentMutationPolicy::clone()
    const {
  return std::unique_ptr<IdempotentMutationPolicy>(
      new SafeIdempotentMutationPolicy(*this));
}

bool SafeIdempotentMutationPolicy::is_idempotent(
    google::bigtable::v2::Mutation const& m) {
  if (!m.has_set_cell()) {
    return true;
  }
  return m.set_cell().timestamp_micros() != ServerSetTimestamp();
}

bool SafeIdempotentMutationPolicy::is_idempotent(
    google::bigtable::v2::CheckAndMutateRowRequest const&) {
  // TODO(#1715): this is overly conservative
  return false;
}

std::unique_ptr<IdempotentMutationPolicy> AlwaysRetryMutationPolicy::clone()
    const {
  return std::unique_ptr<IdempotentMutationPolicy>(
      new AlwaysRetryMutationPolicy(*this));
}

bool AlwaysRetryMutationPolicy::is_idempotent(
    google::bigtable::v2::Mutation const&) {
  return true;
}

bool AlwaysRetryMutationPolicy::is_idempotent(
    google::bigtable::v2::CheckAndMutateRowRequest const&) {
  return true;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
