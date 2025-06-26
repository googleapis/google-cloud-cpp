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

#include "google/cloud/bigtable/internal/retry_context.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

/*
 * Use a test fixture because initializing `ValidateMetadataFixture` once is
 * slightly more performant than creating one per test.
 */
class RetryContextTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;

  /**
   * Simulate receiving server metadata, using the given `OperationContext`.
   *
   * Returns the headers that would be set by the `OperationContext`, before the
   * next RPC.
   *
   * While it may seem odd to simulate the second half of an RPC, and the first
   * half of another, it makes the tests simpler.
   */
  std::multimap<std::string, std::string> SimulateRequest(
      OperationContext& retry_context, RpcMetadata const& server_metadata = {}) {
    grpc::ClientContext c1;
    metadata_fixture_.SetServerMetadata(c1, server_metadata);
    retry_context.PostCall(c1);

    // Return the headers set by the client.
    grpc::ClientContext c2;
    retry_context.PreCall(c2);
    return metadata_fixture_.GetMetadata(c2);
  }
};

TEST_F(RetryContextTest, StartsWithoutBigtableCookies) {
  OperationContext retry_context;

  grpc::ClientContext c;
  retry_context.PreCall(c);
  auto headers = metadata_fixture_.GetMetadata(c);
  EXPECT_THAT(headers, UnorderedElementsAre(Pair("bigtable-attempt", "0")));
}

TEST_F(RetryContextTest, ParrotsBigtableCookies) {
  OperationContext retry_context;

  RpcMetadata server_metadata;
  server_metadata.headers = {
      {"ignored-key-header", "ignored-value"},
      {"x-goog-cbt-cookie-header-only", "header"},
      {"x-goog-cbt-cookie-both", "header"},
  };
  server_metadata.trailers = {
      {"ignored-key-trailer", "ignored-value"},
      {"x-goog-cbt-cookie-trailer-only", "trailer"},
      {"x-goog-cbt-cookie-both", "trailer"},
  };

  auto headers = SimulateRequest(retry_context, server_metadata);
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-header-only", "header"),
                           Pair("x-goog-cbt-cookie-trailer-only", "trailer"),
                           Pair("x-goog-cbt-cookie-both", "trailer"),
                           Pair("bigtable-attempt", "0")));
}

TEST_F(RetryContextTest, Retries) {
  OperationContext retry_context;

  auto headers = SimulateRequest(
      retry_context, {{}, {{"x-goog-cbt-cookie-routing", "request-0"}}});
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-routing", "request-0"),
                           Pair("bigtable-attempt", "0")));

  // Simulate receiving no `RpcMetadata` from the server. We should remember the
  // cookie from the first response.
  headers = SimulateRequest(retry_context, {});
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-routing", "request-0"),
                           Pair("bigtable-attempt", "1")));

  // Simulate receiving a new routing cookie. We should overwrite the cookie
  // from the first response.
  headers = SimulateRequest(retry_context,
                            {{}, {{"x-goog-cbt-cookie-routing", "request-2"}}});
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-routing", "request-2"),
                           Pair("bigtable-attempt", "2")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
