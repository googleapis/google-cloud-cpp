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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IDEMPOTENT_MUTATION_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IDEMPOTENT_MUTATION_POLICY_H_

#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/version.h"
#include <memory>

namespace google {
namespace cloud {
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

/// Return an instance of the default IdempotentMutationPolicy.
std::unique_ptr<IdempotentMutationPolicy> DefaultIdempotentMutationPolicy();

/**
 * Implements a policy that only accepts truly idempotent mutations.
 *
 * This policy accepts only truly idempotent mutations, that is, it rejects
 * mutations where the server sets the timestamp.  Some applications may find
 * this too restrictive and can set their own policies if they wish.
 */
class SafeIdempotentMutationPolicy : public IdempotentMutationPolicy {
 public:
  SafeIdempotentMutationPolicy() {}

  std::unique_ptr<IdempotentMutationPolicy> clone() const override;
  bool is_idempotent(google::bigtable::v2::Mutation const&) override;
};

/**
 * Implements a policy that retries all mutations.
 *
 * Notice that this will may result in non-idempotent mutations being resent
 * to the server.  Re-trying a SetCell() mutation where the server selects the
 * timestamp can result in multiple copies of the data stored with different
 * timestamps.  Only use this policy if your application is prepared to handle
 * such problems, for example, by only querying the last value and setting
 * garbage collection policies to delete the old values.
 */
class AlwaysRetryMutationPolicy : public IdempotentMutationPolicy {
 public:
  AlwaysRetryMutationPolicy() {}

  std::unique_ptr<IdempotentMutationPolicy> clone() const override;
  bool is_idempotent(google::bigtable::v2::Mutation const&) override;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IDEMPOTENT_MUTATION_POLICY_H_
