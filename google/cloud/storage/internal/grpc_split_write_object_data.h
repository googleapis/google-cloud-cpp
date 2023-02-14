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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SPLIT_WRITE_OBJECT_DATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SPLIT_WRITE_OBJECT_DATA_H

#include "google/cloud/storage/internal/const_buffer.h"
#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename ReturnType>
class SplitObjectWriteData;

template <>
class SplitObjectWriteData<std::string> {
 public:
  explicit SplitObjectWriteData(absl::string_view buffer);
  explicit SplitObjectWriteData(
      google::cloud::storage::internal::ConstBufferSequence buffers);

  bool Done() const;
  std::string Next();

 private:
  google::cloud::storage::internal::ConstBufferSequence buffers_;
  std::size_t total_size_;
  std::size_t offset_ = 0;
};

template <>
class SplitObjectWriteData<absl::Cord> {
 public:
  explicit SplitObjectWriteData(absl::string_view buffer);
  explicit SplitObjectWriteData(
      google::cloud::storage::internal::ConstBufferSequence const& buffers);

  bool Done() const;
  absl::Cord Next();

 private:
  absl::Cord cord_;
  std::size_t offset_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_SPLIT_WRITE_OBJECT_DATA_H
