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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_READER_H

#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/async/read_range.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Adapts `ReadRange` to meet the `AsyncReaderConnection` interface.
 *
 * We want to return `AsyncReader` objects from `ObjectDescriptor`. To do so, we
 * need to implement the `AsyncReaderConnection` interface, using `ReadRange` as
 * the underlying implementation.
 */
class ObjectDescriptorReader
    : public storage_experimental::AsyncReaderConnection {
 public:
  explicit ObjectDescriptorReader(std::shared_ptr<ReadRange> impl);
  ~ObjectDescriptorReader() override = default;

  void Cancel() override;
  future<ReadResponse> Read() override;
  RpcMetadata GetRequestMetadata() override;

 private:
  std::shared_ptr<ReadRange> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OBJECT_DESCRIPTOR_READER_H
