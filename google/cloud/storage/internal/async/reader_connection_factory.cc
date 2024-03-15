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

#include "google/cloud/storage/internal/async/reader_connection_factory.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void UpdateGeneration(google::storage::v2::ReadObjectRequest& request,
                      storage::Generation generation) {
  if (request.generation() != 0 || !generation.has_value()) return;
  request.set_generation(generation.value());
}

void UpdateReadRange(google::storage::v2::ReadObjectRequest& request,
                     std::int64_t received_bytes) {
  if (received_bytes <= 0) return;
  if (request.read_limit() != 0) {
    if (request.read_limit() <= received_bytes) {
      // Should not happen, either the service returned more bytes than the
      // limit or we are trying to resume a download that completed
      // successfully. Set the request to a value that will make the next
      // request fail.
      request.set_read_limit(-1);
      return;
    }
    request.set_read_limit(request.read_limit() - received_bytes);
  }
  request.set_read_offset(request.read_offset() + received_bytes);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
