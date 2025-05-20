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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_DESCRIPTOR_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_DESCRIPTOR_CONNECTION_H

#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/storage/v2/storage.pb.h>
#include <cstdint>
#include <memory>

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
class ObjectDescriptorConnection {
 public:
  virtual ~ObjectDescriptorConnection() = default;

  virtual Options options() const = 0;

  /// Returns, if available, the object metadata associated with this
  /// descriptor.
  virtual absl::optional<google::storage::v2::Object> metadata() const = 0;

  /**
   * A thin wrapper around the `Read()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct ReadParams {
    std::int64_t start;
    std::int64_t length;
  };

  /**
   * Starts a new range read in the current descriptor.
   */
  virtual std::unique_ptr<AsyncReaderConnection> Read(ReadParams p) = 0;

  virtual void MakeSubsequentStream() = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_DESCRIPTOR_CONNECTION_H
