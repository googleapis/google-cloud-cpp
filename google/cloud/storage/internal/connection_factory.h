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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CONNECTION_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CONNECTION_FACTORY_H

#include "google/cloud/storage/internal/storage_connection.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class GenericStub;

/**
 * Applies all the decorators configured in @p opts to @p connection.
 *
 * Some existing tests in the storage library (and possibly in customer code),
 * depend on creating a mock connection and then applying all the decorators.
 */
std::shared_ptr<storage::internal::StorageConnection> DecorateConnection(
    Options const& opts,
    std::shared_ptr<storage::internal::StorageConnection> connection);

/**
 * Creates a storage connection object.
 *
 * In most libraries we have no need for such a factory function. Storage has
 * the GCS+gRPC plugin. This plugin needs to create its own stubs and then
 * wrap them with all the decorators, including the `*Connection`. We also want
 * to make the plugin compile-time and link-time optional, so we cannot simply
 * initialize this stub in the usual `MakeStorageConnection(Options)` function.
 */
std::shared_ptr<storage::internal::StorageConnection> MakeStorageConnection(
    Options const& opts, std::unique_ptr<GenericStub> stub);

/// Creates a fully configured connection for the storage service.
std::shared_ptr<storage::internal::StorageConnection> MakeStorageConnection(
    Options const& opts);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CONNECTION_FACTORY_H
