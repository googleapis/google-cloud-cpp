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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
namespace {

TEST(GrpcBufferReadObjectData, SimpleString) {
  GrpcBufferReadObjectData buffer;
  auto const contents =
      std::string{"The quick brown fox jumps over the lazy fox"};
  auto actual = std::string{};

  auto constexpr kBufferSize = 8;
  char buf[kBufferSize];
  auto response = [&] {
    auto n = buffer.HandleResponse(buf, kBufferSize, contents);
    actual.append(buf, buf + n);
    return n;
  };
  auto fill = [&] {
    auto n = buffer.FillBuffer(buf, kBufferSize);
    actual.append(buf, buf + n);
    return n;
  };
  for (auto n = response(); n == kBufferSize; n = fill()) continue;
  EXPECT_EQ(actual, contents);
}

TEST(GrpcBufferReadObjectData, SimpleCord) {
  GrpcBufferReadObjectData buffer;
  auto const contents =
      std::string{"The quick brown fox jumps over the lazy fox"};
  auto actual = std::string{};

  auto constexpr kBufferSize = 8;
  char buf[kBufferSize];
  auto response = [&] {
    auto n = buffer.HandleResponse(buf, kBufferSize, absl::Cord(contents));
    actual.append(buf, buf + n);
    return n;
  };
  auto fill = [&] {
    auto n = buffer.FillBuffer(buf, kBufferSize);
    actual.append(buf, buf + n);
    return n;
  };
  for (auto n = response(); n == kBufferSize; n = fill()) continue;
  EXPECT_EQ(actual, contents);
}

}  // namespace
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
