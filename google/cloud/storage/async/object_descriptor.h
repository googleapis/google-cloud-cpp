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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_DESCRIPTOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_DESCRIPTOR_H

#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/async/reader.h"
#include "google/cloud/storage/async/token.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * `ObjectDescriptor` is analogous to a file descriptor.
 *
 * Applications use an `ObjectDescriptor` to perform multiple reads on the same
 * Google Cloud Storage object.
 */
class ObjectDescriptor {
 public:
  /// Creates an uninitialized descriptor.
  ///
  /// It is UB (undefined behavior) to use any functions on this descriptor.
  ObjectDescriptor() = default;

  /// Initialize a descriptor from its implementation class.
  explicit ObjectDescriptor(std::shared_ptr<ObjectDescriptorConnection> impl)
      : impl_(std::move(impl)) {}

  ///@{
  /// @name Move-only.
  ObjectDescriptor(ObjectDescriptor&&) noexcept = default;
  ObjectDescriptor& operator=(ObjectDescriptor&&) noexcept = default;
  ObjectDescriptor(ObjectDescriptor const&) = delete;
  ObjectDescriptor& operator=(ObjectDescriptor const&) = delete;
  ///@}

  /// Returns, if available, the object metadata associated with this
  /// descriptor.
  absl::optional<google::storage::v2::Object> metadata() const;

  /**
   * Starts a new range read in the current descriptor.
   */
  std::pair<AsyncReader, AsyncToken> Read(std::int64_t offset,
                                          std::int64_t limit);

  /**
   * Starts a new read beginning at the supplied offset and continuing
   * until the end.
   */
  std::pair<AsyncReader, AsyncToken> ReadFromOffset(std::int64_t offset);

  /**
   * Reads the last number of bytes from the end.
   */
  std::pair<AsyncReader, AsyncToken> ReadLast(std::int64_t limit);

 private:
  std::shared_ptr<ObjectDescriptorConnection> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_DESCRIPTOR_H
