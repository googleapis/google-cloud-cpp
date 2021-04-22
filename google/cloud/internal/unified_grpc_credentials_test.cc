// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;

TEST(UnifiedGrpcCredentialsTest, WithGrpcCredentials) {
  auto result =
      CreateAuthenticationStrategy(grpc::InsecureChannelCredentials());
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentials) {
  // Create a filename for a file that (most likely) does not exist. We just
  // want to initialize the default credentials, the filename won't be used by
  // the test.
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", "unused.json");

  auto result = CreateAuthenticationStrategy(MakeGoogleDefaultCredentials());
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithAccessTokenCredentials) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::hours(1);
  auto result = CreateAuthenticationStrategy(
      MakeAccessTokenCredentials("test-token", expiration));
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_NE(nullptr, context.credentials());
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
