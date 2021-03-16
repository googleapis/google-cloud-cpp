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

#include "google/cloud/internal/log_impl.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

auto constexpr kLogConfig = "GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG";
auto constexpr kEnableClog = "GOOGLE_CLOUD_CPP_ENABLE_CLOG";

TEST(CircularBufferBackend, Basic) {
  auto be = std::make_shared<ScopedLog::Backend>();
  CircularBufferBackend buffer(3, Severity::GCP_LS_ERROR, be);
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_INFO, "test_function()", "file", 1, {}, "msg 1"});
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_DEBUG, "test_function()", "file", 1, {}, "msg 2"});
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_TRACE, "test_function()", "file", 1, {}, "msg 3"});
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_WARNING, "test_function()", "file", 1, {}, "msg 4"});
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_INFO, "test_function()", "file", 1, {}, "msg 5"});
  EXPECT_THAT(be->ExtractLines(), IsEmpty());

  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_ERROR, "test_error()", "file", 1, {}, "msg 6"});
  EXPECT_THAT(be->ExtractLines(), ElementsAre("msg 4", "msg 5", "msg 6"));

  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_INFO, "test_function()", "file", 1, {}, "msg 7"});
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_ERROR, "test_function()", "file", 1, {}, "msg 8"});
  EXPECT_THAT(be->ExtractLines(), ElementsAre("msg 7", "msg 8"));

  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_INFO, "test_function()", "file", 1, {}, "msg 9"});
  buffer.ProcessWithOwnership(LogRecord{
      Severity::GCP_LS_INFO, "test_function()", "file", 1, {}, "msg 10"});
  buffer.Flush();
  EXPECT_THAT(be->ExtractLines(), ElementsAre("msg 9", "msg 10"));
}

TEST(DefaultLogBackend, CircularBuffer) {
  ScopedEnvironment config(kLogConfig, "lastN,5,WARNING");
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_TRUE(!!be);
  auto const* buffer = dynamic_cast<CircularBufferBackend*>(be.get());
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(5, buffer->size());
  EXPECT_EQ(Severity::GCP_LS_WARNING, buffer->min_flush_severity());
}

TEST(DefaultLogBackend, CLog) {
  ScopedEnvironment config(kLogConfig, "clog");
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_TRUE(!!be);
  EXPECT_NE(dynamic_cast<StdClogBackend*>(be.get()), nullptr);
}

TEST(DefaultLogBackend, CLogBackwardsCompatibility) {
  ScopedEnvironment config(kLogConfig, absl::nullopt);
  ScopedEnvironment clog(kEnableClog, "yes");
  auto be = DefaultLogBackend();
  EXPECT_TRUE(!!be);
  EXPECT_NE(dynamic_cast<StdClogBackend*>(be.get()), nullptr);
}

TEST(DefaultLogBackend, Unset) {
  ScopedEnvironment config(kLogConfig, absl::nullopt);
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_FALSE(!!be);
}

TEST(DefaultLogBackend, UnknownType) {
  ScopedEnvironment config(kLogConfig, "invalid");
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_FALSE(!!be);
}

TEST(DefaultLogBackend, MissingComponents) {
  ScopedEnvironment config(kLogConfig, "lastN,1");
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_FALSE(!!be);
}

TEST(DefaultLogBackend, InvalidSize) {
  ScopedEnvironment config(kLogConfig, "lastN,-10,WARNING");
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_FALSE(!!be);
}

TEST(DefaultLogBackend, InvalidSeverity) {
  ScopedEnvironment config(kLogConfig, "lastN,10,invalid");
  ScopedEnvironment clog(kEnableClog, absl::nullopt);
  auto be = DefaultLogBackend();
  EXPECT_FALSE(!!be);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
