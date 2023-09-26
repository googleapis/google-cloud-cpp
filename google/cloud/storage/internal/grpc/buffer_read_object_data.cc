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

#include "google/cloud/storage/internal/grpc/buffer_read_object_data.h"
#include "google/cloud/storage/internal/grpc/make_cord.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace storage_internal {

std::size_t GrpcBufferReadObjectData::FillBuffer(char* buffer, std::size_t n) {
  std::size_t offset = 0;
  for (auto v : contents_.Chunks()) {
    if (offset == n) break;
    auto const count = std::min(v.size(), n - offset);
    std::copy(v.data(), v.data() + count, buffer + offset);
    offset += count;
  }
  contents_.RemovePrefix(offset);
  return offset;
}

std::size_t GrpcBufferReadObjectData::HandleResponse(char* buffer,
                                                     std::size_t n,
                                                     std::string contents) {
  return HandleResponse(buffer, n, MakeCord(std::move(contents)));
}

std::size_t GrpcBufferReadObjectData::HandleResponse(char* buffer,
                                                     std::size_t n,
                                                     absl::Cord contents) {
  contents_ = std::move(contents);
  return FillBuffer(buffer, n);
}

}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
