// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CONNECTION_H

#include "google/cloud/storage/async_object_responses.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// TODO(#7142) - move to the public API when we fix the mocking story for GCS
/**
 * The `*Connection` object for `storage_experimental::AsyncClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `storage_experimental::AsyncClient`. This allows users to inject
 * custom behavior (e.g., with a Google Mock object) when writing tests that use
 * objects of type `storage_experimental::AsyncClient`.
 *
 * To create a concrete instance, see `MakeAsyncConnection()`.
 *
 * For mocking, see `storage_mocks::MockAsyncConnection`.
 */
class AsyncConnection {
 public:
  virtual ~AsyncConnection() = default;

  virtual Options options() const = 0;

  virtual future<storage_experimental::AsyncReadObjectRangeResponse>
  AsyncReadObjectRange(storage::internal::ReadObjectRangeRequest request) = 0;

  virtual future<Status> AsyncDeleteObject(
      storage::internal::DeleteObjectRequest request) = 0;

  virtual future<StatusOr<std::string>> AsyncStartResumableWrite(
      storage::internal::ResumableUploadRequest request) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CONNECTION_H
