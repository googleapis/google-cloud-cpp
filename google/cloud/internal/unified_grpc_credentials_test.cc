// Copyright 2021 Google LLC
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

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pair;

TEST(UnifiedGrpcCredentialsTest, GrpcCredentialOption) {
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      cq,
      Options{}.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()));
  EXPECT_FALSE(result->RequiresConfigureContext());
}

TEST(UnifiedGrpcCredentialsTest, UnifiedCredentialsOption) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::hours(1);
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      cq, Options{}
              .set<UnifiedCredentialsOption>(
                  MakeAccessTokenCredentials("test-token", expiration))
              .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials()));
  // Verify `UnifiedCredentialsOption` is used before `GrpcCredentialOption`
  EXPECT_TRUE(result->RequiresConfigureContext());
}

TEST(UnifiedGrpcCredentialsTest, WithGrpcCredentials) {
  auto result =
      CreateAuthenticationStrategy(grpc::InsecureChannelCredentials());
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithInsecureCredentials) {
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(*MakeInsecureCredentials(), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithDefaultCredentials) {
  // Create a filename for a file that (most likely) does not exist. We just
  // want to initialize the default credentials, the filename won't be used by
  // the test.
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", "unused.json");

  CompletionQueue cq;
  auto result =
      CreateAuthenticationStrategy(*MakeGoogleDefaultCredentials(), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_EQ(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithAccessTokenCredentials) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::hours(1);
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      *MakeAccessTokenCredentials("test-token", expiration), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_NE(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, WithErrorCredentials) {
  Status const error_status{StatusCode::kFailedPrecondition,
                            "Precondition failed."};
  CompletionQueue cq;
  auto result = CreateAuthenticationStrategy(
      *internal::MakeErrorCredentials(error_status), cq);
  ASSERT_NE(nullptr, result.get());
  EXPECT_TRUE(result->RequiresConfigureContext());
  grpc::ClientContext context;
  auto configured_context = result->ConfigureContext(context);
  EXPECT_THAT(configured_context, StatusIs(error_status.code()));
  auto async_configured_context =
      result->AsyncConfigureContext(std::make_shared<grpc::ClientContext>())
          .get();
  EXPECT_THAT(async_configured_context, StatusIs(error_status.code()));
  auto channel = result->CreateChannel(std::string{}, grpc::ChannelArguments{});
  EXPECT_THAT(channel.get(), Not(IsNull()));
}

TEST(UnifiedGrpcCredentialsTest, WithApiKeyCredentials) {
  CompletionQueue cq;
  auto creds = ApiKeyConfig("api-key", Options{});
  auto auth = CreateAuthenticationStrategy(creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  auto status = auth->ConfigureContext(context);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(context.credentials(), IsNull());

  ValidateMetadataFixture fixture;
  auto headers = fixture.GetMetadata(context);
  EXPECT_THAT(headers, Contains(Pair("x-goog-api-key", "api-key")));
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoNotSet) {
  auto contents = LoadCAInfo(Options{});
  EXPECT_FALSE(contents.has_value());
}

std::string CreateRandomFileName() {
  static DefaultPRNG generator = MakeDefaultPRNG();
  // When running on the internal Google CI systems we cannot write to the local
  // directory, GTest has a good temporary directory in that case.
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(),
      Sample(generator, 8, "abcdefghijklmnopqrstuvwxyz0123456789"));
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoNotExist) {
  auto filename = CreateRandomFileName();
  auto contents = LoadCAInfo(Options{}.set<CARootsFilePathOption>(filename));
  ASSERT_TRUE(contents.has_value());
  EXPECT_THAT(*contents, IsEmpty());
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoContents) {
  auto filename = CreateRandomFileName();
  auto const expected =
      std::string{"The quick brown fox jumps over the lazy dog"};
  std::ofstream(filename) << expected;
  auto contents = LoadCAInfo(Options{}.set<CARootsFilePathOption>(filename));
  (void)std::remove(filename.c_str());  // remove the temporary file
  ASSERT_TRUE(contents.has_value());
  EXPECT_EQ(*contents, expected);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
