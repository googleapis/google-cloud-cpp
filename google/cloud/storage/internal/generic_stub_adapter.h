// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_STUB_ADAPTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_STUB_ADAPTER_H

#include "google/cloud/storage/internal/generic_stub.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class StorageConnection;

}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage

namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Adapts existing `storage::internal::StorageConnection` decorators to the
 * `GenericStub` interface.
 *
 * This function should be short-lived, as we transition some decorators
 * `storage::internal::StorageConnection` to `storage::GenericStub`. The PRs are
 * going to be large as it is, this adapter will let us change one class at a
 * time without being cruel to the reviewers.
 */
std::unique_ptr<GenericStub> MakeGenericStubAdapter(
    std::shared_ptr<storage::internal::StorageConnection>);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_STUB_ADAPTER_H
