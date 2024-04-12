// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_PAYLOAD_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_PAYLOAD_IMPL_H

#include "google/cloud/storage/async/write_payload.h"
#include "google/cloud/storage/internal/grpc/make_cord.h"
#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include <numeric>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Groups helpers to access implementation details in
/// `storage_experimental::WritePayload`.
struct WritePayloadImpl {
  static storage_experimental::WritePayload Make(absl::Cord impl) {
    return storage_experimental::WritePayload(std::move(impl));
  }
  static absl::Cord GetImpl(storage_experimental::WritePayload const& p) {
    return p.impl_;
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_PAYLOAD_IMPL_H
