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

#ifndef BIGTABLE_CLIENT_IDEMPOTENT_MUTATION_POLICY_H_
#define BIGTABLE_CLIENT_IDEMPOTENT_MUTATION_POLICY_H_

#include <bigtable/client/version.h>

#include <memory>
#include "bigtable/client/mutations.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Defines the interface to control which mutations are idempotent and therefore
 * can be re-tried.
 */
class IdempotentMutationPolicy {
 public:
  virtual ~IdempotentMutationPolicy() = default;

  /// Return a copy of the policy.
  virtual std::unique_ptr<IdempotentMutationPolicy> clone() const = 0;

  /// Return true if the mutation is idempotent.
  virtual bool is_idempotent(google::bigtable::v2::Mutation const&) = 0;
};

/// Return an instance of the default MutationRetryPolicy.
std::unique_ptr<IdempotentMutationPolicy> DefaultIdempotentMutationPolicy();

/**
 * Implements a safe policy to determine if a mutation is idempotent.
 */
class SafeIdempotentMutationPolicy : public IdempotentMutationPolicy {
 public:
  SafeIdempotentMutationPolicy() {}

  std::unique_ptr<IdempotentMutationPolicy> clone() const override;
  bool is_idempotent(google::bigtable::v2::Mutation const&) override;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_IDEMPOTENT_MUTATION_POLICY_H_
