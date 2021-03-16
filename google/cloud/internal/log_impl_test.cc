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
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::internal::CircularBufferBackend;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

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

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
