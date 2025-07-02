// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/object_descriptor_reader.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ObjectDescriptorReader::ObjectDescriptorReader(std::shared_ptr<ReadRange> impl)
    : impl_(std::move(impl)) {}

void ObjectDescriptorReader::Cancel() { /* noop, cannot cancel a single range
                                           read */
}

future<ObjectDescriptorReader::ReadResponse> ObjectDescriptorReader::Read() {
  return impl_->Read();
}

RpcMetadata ObjectDescriptorReader::GetRequestMetadata() { return {}; }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
