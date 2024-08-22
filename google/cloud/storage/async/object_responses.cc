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

#include "google/cloud/storage/async/object_responses.h"
#include "google/cloud/storage/internal/async/write_payload_impl.h"
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ReadPayload::ReadPayload(std::string contents)
    : impl_(storage_internal::MakeCord(std::move(contents))) {}

ReadPayload::ReadPayload(std::vector<std::string> contents) {
  for (auto& v : contents) {
    impl_.Append(storage_internal::MakeCord(std::move(v)));
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
