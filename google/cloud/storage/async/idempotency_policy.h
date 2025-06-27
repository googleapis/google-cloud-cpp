// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_IDEMPOTENCY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_IDEMPOTENCY_POLICY_H

#include "google/cloud/idempotency.h"
#include "google/cloud/version.h"
#include "google/storage/v2/storage.pb.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Define the interface for the `AsyncClient`'s idempotency policy.
 *
 * The idempotency policy controls which requests are treated as idempotent and
 * therefore safe to retry on a transient failure. Retrying non-idempotent
 * operations can result in data loss.
 *
 * Consider, for example, `DeleteObject()`. If this operation is called without
 * pre-conditions retrying it may delete more than one version of an object.
 *
 * Even operations that "add" data can result in data loss. Consider, as another
 * example, inserting a new object. If called without pre-conditions retrying
 * this operation will insert multiple new versions. If the bucket is configured
 * to only keep the last N versions of each object, then the retry would have
 * deleted more data than desired.
 *
 * Some applications are designed to handle duplicate requests without data
 * loss, or the library may be used in an environment where the risk of data
 * loss due to duplicate requests is negligible or zero.
 *
 * This policy allows application developers to control the behavior of the
 * library with respect to retrying non-idempotent operations. Application
 * developers can configure the library to only retry operations that are known
 * to be idempotent (that is, they will succeed only once). Application may also
 * configure the library to retry all operations, regardless of whether the
 * operations are idempotent or not.
 */
class IdempotencyPolicy {
 public:
  virtual ~IdempotencyPolicy() = 0;

  /// Determine if a google.storage.v2.ReadObjectRequest is idempotent.
  virtual google::cloud::Idempotency ReadObject(
      google::storage::v2::ReadObjectRequest const&);

  /// Determine if a google.storage.v2.WriteObjectRequest for a one-shot upload
  /// is idempotent.
  virtual google::cloud::Idempotency InsertObject(
      google::storage::v2::WriteObjectRequest const&);

  /// Determine if a google.storage.v2.WriteObjectRequest for a resumable upload
  /// is idempotent.
  virtual google::cloud::Idempotency WriteObject(
      google::storage::v2::WriteObjectRequest const&);

  /// Determine if a google.storage.v2.ComposeObjectRequest is idempotent.
  virtual google::cloud::Idempotency ComposeObject(
      google::storage::v2::ComposeObjectRequest const&);

  /// Determine if a google.storage.v2.DeleteObjectRequest is idempotent.
  virtual google::cloud::Idempotency DeleteObject(
      google::storage::v2::DeleteObjectRequest const&);

  /// Determine if a google.storage.v2.RewriteObjectRequest is idempotent.
  virtual google::cloud::Idempotency RewriteObject(
      google::storage::v2::RewriteObjectRequest const&);
};

/// Creates an idempotency policy where only safe operations are retried.
std::unique_ptr<IdempotencyPolicy> MakeStrictIdempotencyPolicy();

/// Creates an idempotency policy that retries all operations.
std::unique_ptr<IdempotencyPolicy> MakeAlwaysRetryIdempotencyPolicy();

/**
 * An option (see `google::cloud::Options`) to set the idempotency policy.
 */
struct IdempotencyPolicyOption {
  using Type = std::function<std::unique_ptr<IdempotencyPolicy>()>;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_IDEMPOTENCY_POLICY_H
