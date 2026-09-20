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

#include "google/cloud/spanner/internal/spanner_request_id.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <regex>
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
using ::testing::Gt;
using ::testing::Ne;

MATCHER_P(MatchesStdRegex, pattern, "") {
  if (std::regex_match(arg, std::regex(pattern))) {
    return true;
  }
  *result_listener << "which does not match regex \"" << pattern << "\"";
  return false;
}

TEST(SpannerRequestIdTest, ProcessRandomIdFormat) {
  std::string const id1 = ProcessRandomId();
  EXPECT_THAT(id1, MatchesStdRegex("^[0-9a-f]{16}$"));
  std::string const id2 = ProcessRandomId();
  EXPECT_THAT(id2, Eq(id1));
}

TEST(SpannerRequestIdTest, NextClientIdMonotonic) {
  std::uint64_t const c1 = NextClientId();
  std::uint64_t const c2 = NextClientId();
  std::uint64_t const c3 = NextClientId();
  EXPECT_THAT(c1, Gt(0ULL));
  EXPECT_THAT(c2, Eq(c1 + 1));
  EXPECT_THAT(c3, Eq(c2 + 1));
}

TEST(SpannerRequestIdTest, FormatSpannerRequestStaticPrefix) {
  std::string const prefix =
      FormatSpannerRequestStaticPrefix(1, "0123456789abcdef", 42);
  EXPECT_THAT(prefix, Eq("1.0123456789abcdef.42."));
}

TEST(SpannerRequestIdTest, FormatSpannerRequestIdWithPrefix) {
  std::string const prefix = "1.0123456789abcdef.42.";
  std::string const request_id = FormatSpannerRequestId(prefix, 1, 100, 2);
  EXPECT_THAT(request_id, Eq("1.0123456789abcdef.42.1.100.2"));
}

TEST(SpannerRequestIdTest, FormatSpannerRequestIdDirect) {
  std::string const request_id =
      FormatSpannerRequestId(1, "0123456789abcdef", 42, 3, 200, 1);
  EXPECT_THAT(request_id, Eq("1.0123456789abcdef.42.3.200.1"));
}

#ifndef _WIN32
TEST(SpannerRequestIdTest, ProcessRandomIdForkRegeneration) {
  // Ensure the parent has already initialized ProcessRandomId
  std::string const parent_id = ProcessRandomId();
  ASSERT_THAT(parent_id, MatchesStdRegex("^[0-9a-f]{16}$"));

  int pipe_fds[2];
  ASSERT_THAT(pipe(pipe_fds), Eq(0));

  pid_t const pid = fork();
  ASSERT_THAT(pid, Ge(0));

  if (pid == 0) {
    // Child process: read ProcessRandomId and write to pipe
    close(pipe_fds[0]);
    std::string const child_id = ProcessRandomId();
    write(pipe_fds[1], child_id.data(), child_id.size());
    close(pipe_fds[1]);
    _exit(0);
  }

  // Parent process: read child's ID from pipe
  close(pipe_fds[1]);
  char buffer[32] = {0};
  ssize_t const bytes_read = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
  close(pipe_fds[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_THAT(WEXITSTATUS(status), Eq(0));

  ASSERT_THAT(bytes_read, Eq(16));
  std::string const child_id(buffer, static_cast<std::size_t>(bytes_read));
  EXPECT_THAT(child_id, MatchesStdRegex("^[0-9a-f]{16}$"));
  EXPECT_THAT(child_id, Ne(parent_id));
}
#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
