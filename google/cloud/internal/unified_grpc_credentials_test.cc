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
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsEmpty;

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
  auto result = CreateAuthenticationStrategy(MakeInsecureCredentials(), cq);
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
      CreateAuthenticationStrategy(MakeGoogleDefaultCredentials(), cq);
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
      MakeAccessTokenCredentials("test-token", expiration), cq);
  ASSERT_NE(nullptr, result.get());
  grpc::ClientContext context;
  auto status = result->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());
  ASSERT_NE(nullptr, context.credentials());
}

TEST(UnifiedGrpcCredentialsTest, LoadCAInfoNotSet) {
  auto contents = LoadCAInfo(Options{});
  EXPECT_FALSE(contents.has_value());
}

std::string CreateRandomFileName() {
  static DefaultPRNG generator = MakeDefaultPRNG();
  // When running on the internal Google CI systems we cannot write to the local
  // directory, GTest has a good temporary directory in that case.
  return ::testing::TempDir() +
         Sample(generator, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
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
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
