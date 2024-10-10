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

#include "google/cloud/internal/grpc_api_key_authentication.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pair;

TEST(GrpcApiKeyAuthenticationTest, CreateChannel) {
  GrpcApiKeyAuthentication auth("api-key");
  EXPECT_THAT(auth.CreateChannel("localhost:1", grpc::ChannelArguments{}),
              NotNull());
}

TEST(GrpcApiKeyAuthenticationTest, ConfigureContext) {
  GrpcApiKeyAuthentication auth("api-key");
  EXPECT_TRUE(auth.RequiresConfigureContext());

  grpc::ClientContext context;
  EXPECT_STATUS_OK(auth.ConfigureContext(context));
  EXPECT_THAT(context.credentials(), IsNull());

  ValidateMetadataFixture fixture;
  auto headers = fixture.GetMetadata(context);
  EXPECT_THAT(headers, Contains(Pair("x-goog-api-key", "api-key")));
}

TEST(GrpcApiKeyAuthenticationTest, AsyncConfigureContext) {
  GrpcApiKeyAuthentication auth("api-key");
  EXPECT_TRUE(auth.RequiresConfigureContext());

  auto context =
      auth.AsyncConfigureContext(std::make_shared<grpc::ClientContext>()).get();
  ASSERT_STATUS_OK(context);

  ValidateMetadataFixture fixture;
  auto headers = fixture.GetMetadata(**context);
  EXPECT_THAT(headers, Contains(Pair("x-goog-api-key", "api-key")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
