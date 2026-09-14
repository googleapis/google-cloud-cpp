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

#include "google/cloud/spanner/internal/spanner_operation_context_factory.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;
using ::testing::Ge;
using ::testing::MatchesRegex;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::StrEq;

TEST(SpannerOperationContextFactoryTest, CounterIsolationAndRpcNames) {
  auto process_random_id =
      std::make_shared<std::string const>("0123456789abcdef");
  DefaultSpannerOperationContextFactory factory(100, process_random_id);

  // User RPCs
  auto ctx_create_session = factory.CreateSession();
  EXPECT_THAT(ctx_create_session.request_index(), Eq(1ULL));
  EXPECT_THAT(ctx_create_session.rpc_name(), StrEq("CreateSession"));

  auto ctx_exec_sql = factory.ExecuteSql();
  EXPECT_THAT(ctx_exec_sql.request_index(), Eq(2ULL));
  EXPECT_THAT(ctx_exec_sql.rpc_name(), StrEq("ExecuteSql"));

  auto ctx_commit = factory.Commit();
  EXPECT_THAT(ctx_commit.request_index(), Eq(3ULL));
  EXPECT_THAT(ctx_commit.rpc_name(), StrEq("Commit"));

  // Background RPCs
  auto bg_create_session = factory.BackgroundCreateSession();
  EXPECT_THAT(bg_create_session.request_index(), Eq(1ULL));
  EXPECT_THAT(bg_create_session.rpc_name(), StrEq("BackgroundCreateSession"));

  auto bg_batch_create = factory.BackgroundBatchCreateSessions();
  EXPECT_THAT(bg_batch_create.request_index(), Eq(2ULL));
  EXPECT_THAT(bg_batch_create.rpc_name(),
              StrEq("BackgroundBatchCreateSessions"));

  auto bg_delete = factory.BackgroundDeleteSession();
  EXPECT_THAT(bg_delete.request_index(), Eq(3ULL));
  EXPECT_THAT(bg_delete.rpc_name(), StrEq("BackgroundDeleteSession"));

  auto bg_refresh = factory.BackgroundRefreshSession();
  EXPECT_THAT(bg_refresh.request_index(), Eq(4ULL));
  EXPECT_THAT(bg_refresh.rpc_name(), StrEq("BackgroundRefreshSession"));

  // Next user RPC resumes user counter without interference from background
  // counter
  auto ctx_rollback = factory.Rollback();
  EXPECT_THAT(ctx_rollback.request_index(), Eq(4ULL));
  EXPECT_THAT(ctx_rollback.rpc_name(), StrEq("Rollback"));
}

TEST(SpannerOperationContextFactoryTest, HeaderGenerationFromFactory) {
  auto process_random_id =
      std::make_shared<std::string const>("0123456789abcdef");
  DefaultSpannerOperationContextFactory factory(5, process_random_id);

  auto op_context = factory.ExecuteSql();
  op_context.BindChannel(2);

  grpc::ClientContext client_context;
  op_context.PreCall(client_context);

  EXPECT_THAT(op_context.RequestId(),
              Optional(Eq("1.0123456789abcdef.5.2.1.1")));
}

#ifndef _WIN32
TEST(SpannerOperationContextFactoryTest,
     ForkDetectsAndRegeneratesStaticPrefix) {
  auto process_random_id =
      std::make_shared<std::string const>("0123456789abcdef");
  DefaultSpannerOperationContextFactory factory(7, process_random_id);

  auto parent_ctx = factory.ExecuteSql();
  grpc::ClientContext parent_client_context;
  parent_ctx.PreCall(parent_client_context);
  EXPECT_THAT(parent_ctx.RequestId(),
              Optional(Eq("1.0123456789abcdef.7.0.1.1")));

  int pipe_fds[2];
  ASSERT_THAT(pipe(pipe_fds), Eq(0));

  pid_t const pid = fork();
  ASSERT_THAT(pid, Ge(0));

  if (pid == 0) {
    close(pipe_fds[0]);
    auto child_ctx = factory.ExecuteSql();
    grpc::ClientContext child_client_context;
    child_ctx.PreCall(child_client_context);
    auto const child_request_id = child_ctx.RequestId().value_or("");
    write(pipe_fds[1], child_request_id.data(), child_request_id.size());
    close(pipe_fds[1]);
    _exit(0);
  }

  close(pipe_fds[1]);
  char buffer[128] = {0};
  ssize_t const bytes_read = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
  close(pipe_fds[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  ASSERT_THAT(WIFEXITED(status), Eq(true));
  ASSERT_THAT(WEXITSTATUS(status), Eq(0));

  std::string const child_request_id(buffer,
                                     static_cast<std::size_t>(bytes_read));
  EXPECT_THAT(child_request_id,
              MatchesRegex("^1\\.[0-9a-f]{16}\\.7\\.0\\.2\\.1$"));
  EXPECT_THAT(child_request_id, Ne("1.0123456789abcdef.7.0.2.1"));
}
#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
