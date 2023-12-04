// Copyright 2022 Google LLC
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

#include "google/cloud/internal/grpc_request_metadata.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(GrpcRequestMetadata, FormatForLoggingDecorator) {
  struct Test {
    RpcMetadata metadata;
    std::string expected;
  } const cases[] = {
      {RpcMetadata{{}, {}}, "headers={}, trailers={}"},
      {RpcMetadata{{{"a", "b"}}, {}}, "headers={{a: b}}, trailers={}"},
      {RpcMetadata{{}, {{"a", "b"}}}, "headers={}, trailers={{a: b}}"},
      {RpcMetadata{{{"a", "b"}, {"k", "v"}}, {{"d", "e"}, {"h", "f"}}},
       "headers={{a: b}, {k: v}}, trailers={{d: e}, {h: f}}"},
  };
  for (auto const& test : cases) {
    auto const actual = FormatForLoggingDecorator(test.metadata);
    EXPECT_EQ(test.expected, actual);
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
