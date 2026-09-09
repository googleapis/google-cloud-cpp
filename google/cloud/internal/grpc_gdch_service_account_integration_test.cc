// Copyright 2026 Google LLC
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

#include "google/cloud/completion_queue.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#if __has_include(<grpcpp/version_info.h>)
#include <grpcpp/version_info.h>
#endif
#include <fstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

TEST(GrpcGdchServiceAccountIntegrationTest,
     RetrievesBearerTokenFromMemoryInAdhocEnvironment) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  std::optional<std::string> key_file_env = GetEnv("GRPC_TEST_GDCH_KEY_FILE");
  std::optional<std::string> audience_env = GetEnv("GRPC_TEST_GDCH_AUDIENCE");
  if (!key_file_env.has_value() || !audience_env.has_value()) GTEST_SKIP();

  std::ifstream is(*key_file_env);
  std::string contents = std::string{std::istreambuf_iterator<char>{is}, {}};
  ASSERT_THAT(contents, Not(IsEmpty()));

  CompletionQueue cq;

  std::shared_ptr<Credentials> creds =
      MakeGDCHServiceAccountCredentials(contents, *audience_env);
  ASSERT_THAT(creds, NotNull());

  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());

  std::shared_ptr<grpc::Channel> channel =
      auth->CreateChannel("localhost:443", grpc::ChannelArguments{});
  EXPECT_THAT(channel, NotNull());
}

TEST(GrpcGdchServiceAccountIntegrationTest,
     RetrievesBearerTokenFromFileInAdhocEnvironment) {
#if !defined(GRPC_CPP_VERSION_MAJOR) || \
    (GRPC_CPP_VERSION_MAJOR < 1 ||      \
     (GRPC_CPP_VERSION_MAJOR == 1 && GRPC_CPP_VERSION_MINOR < 84))
  GTEST_SKIP() << "GDCH credentials require gRPC >= 1.84.0";
#endif
  std::optional<std::string> key_file_env = GetEnv("GRPC_TEST_GDCH_KEY_FILE");
  std::optional<std::string> audience_env = GetEnv("GRPC_TEST_GDCH_AUDIENCE");
  if (!key_file_env.has_value() || !audience_env.has_value()) GTEST_SKIP();

  std::ifstream is(*key_file_env);
  std::string contents = std::string{std::istreambuf_iterator<char>{is}, {}};
  ASSERT_THAT(contents, Not(IsEmpty()));

  CompletionQueue cq;

  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", key_file_env);
  std::shared_ptr<Credentials> creds =
      MakeGDCHServiceAccountCredentials(*audience_env);
  ASSERT_THAT(creds, NotNull());

  std::shared_ptr<GrpcAuthenticationStrategy> auth =
      CreateAuthenticationStrategy(*creds, cq);
  ASSERT_THAT(auth, NotNull());

  grpc::ClientContext context;
  Status status = auth->ConfigureContext(context);
  EXPECT_THAT(status, IsOk());

  std::shared_ptr<grpc::Channel> channel =
      auth->CreateChannel("localhost:443", grpc::ChannelArguments{});
  EXPECT_THAT(channel, NotNull());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
