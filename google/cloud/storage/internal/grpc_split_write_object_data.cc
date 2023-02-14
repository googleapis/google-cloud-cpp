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

#include "google/cloud/storage/internal/grpc_split_write_object_data.h"
#include <google/storage/v2/storage.pb.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

SplitObjectWriteData<std::string>::SplitObjectWriteData(
    absl::string_view buffer)
    : buffers_({buffer}),
      total_size_(storage::internal::TotalBytes(buffers_)) {}

SplitObjectWriteData<std::string>::SplitObjectWriteData(
    google::cloud::storage::internal::ConstBufferSequence buffers)
    : buffers_(std::move(buffers)),
      total_size_(storage::internal::TotalBytes(buffers_)) {}

bool SplitObjectWriteData<std::string>::Done() const {
  return offset_ >= total_size_;
}

std::string SplitObjectWriteData<std::string>::Next() {
  auto constexpr kMax = static_cast<std::size_t>(
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
  std::string result;
  auto start = offset_;
  for (auto const& b : buffers_) {
    if (start >= b.size()) {
      start -= b.size();
      continue;
    }
    auto n = (std::min)(kMax - result.size(), b.size() - start);
    result.append(b.begin() + start, b.begin() + start + n);
    offset_ += n;
    start = 0;
    if (result.size() >= kMax) return result;
  }
  return result;
}

SplitObjectWriteData<absl::Cord>::SplitObjectWriteData(absl::string_view buffer)
    : cord_(buffer) {}

SplitObjectWriteData<absl::Cord>::SplitObjectWriteData(
    google::cloud::storage::internal::ConstBufferSequence const& buffers) {
  for (auto const& b : buffers) {
    cord_.Append(absl::string_view{b.data(), b.size()});
  }
}

bool SplitObjectWriteData<absl::Cord>::Done() const {
  return offset_ >= cord_.size();
}

absl::Cord SplitObjectWriteData<absl::Cord>::Next() {
  auto constexpr kMax = static_cast<std::size_t>(
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
  auto const n = (std::min)(kMax, cord_.size() - offset_);
  auto result = cord_.Subcord(offset_, n);
  offset_ += n;
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
