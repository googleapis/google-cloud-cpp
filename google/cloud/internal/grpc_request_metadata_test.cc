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
#include "google/cloud/internal/status_utils.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(GrpcRequestMetadata, GetRequestMetadataFromContext) {
  auto const server_metadata =
      RpcMetadata{{{"header1", "value1"}, {"header2", "value2"}},
                  {{"trailer1", "value3"}, {"trailer2", "value4"}}};
  grpc::ClientContext context;
  testing_util::SetServerMetadata(context, server_metadata);

  auto md =
      GetRequestMetadataFromContext(context,
                                    /*error_origin=*/ErrorOrigin::kUnknown);
  EXPECT_THAT(md.headers,
              UnorderedElementsAre(
                  Pair("header1", "value1"), Pair("header2", "value2"),
                  // This function also returns the peer and compression
                  // algorithm as synthetic headers.
                  Pair(":grpc-context-peer", _),
                  Pair(":grpc-context-compression-algorithm", "identity")));
  EXPECT_THAT(md.trailers, UnorderedElementsAre(Pair("trailer1", "value3"),
                                                Pair("trailer2", "value4")));
}

TEST(GrpcRequestMetadata,
     GetRequestMetadataFromContextWhenInitialMetadataIsNotReady) {
  auto const server_metadata =
      RpcMetadata{{{"header1", "value1"}, {"header2", "value2"}},
                  {{"trailer1", "value3"}, {"trailer2", "value4"}}};
  grpc::ClientContext context;

  auto md =
      GetRequestMetadataFromContext(context,
                                    /*error_origin=*/ErrorOrigin::kClient);
  EXPECT_THAT(md.headers,
              UnorderedElementsAre(
                  // This function also returns the peer and compression
                  // algorithm as synthetic headers.
                  Pair(":grpc-context-peer", _),
                  Pair(":grpc-context-compression-algorithm", "identity")));
  EXPECT_THAT(md.trailers, IsEmpty());
}

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
