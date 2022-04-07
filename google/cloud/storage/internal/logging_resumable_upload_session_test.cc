// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/logging_resumable_upload_session.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ContainsOnce;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

class LoggingResumableUploadSessionTest : public ::testing::Test {
 protected:
  testing_util::ScopedLog log_backend_;
};

TEST_F(LoggingResumableUploadSessionTest, UploadChunk) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  std::string const payload = "test-payload-data";
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
    EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
    return StatusOr<ResumableUploadResponse>(
        AsStatus(HttpResponse{503, "uh oh", {}}));
  });

  LoggingResumableUploadSession session(std::move(mock));

  auto result = session.UploadChunk({{payload}});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable, "uh oh"));

  EXPECT_THAT(log_backend_.ExtractLines(),
              ContainsOnce(HasSubstr("UNAVAILABLE:")));
}

TEST_F(LoggingResumableUploadSessionTest, UploadFinalChunk) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  std::string const payload = "test-payload-data";
  EXPECT_CALL(*mock, UploadFinalChunk)
      .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s,
                    HashValues const&) {
        EXPECT_THAT(p, ElementsAre(ConstBuffer{payload}));
        EXPECT_EQ(513 * 1024, s);
        return StatusOr<ResumableUploadResponse>(
            AsStatus(HttpResponse{503, "uh oh", {}}));
      });

  LoggingResumableUploadSession session(std::move(mock));

  auto result = session.UploadFinalChunk({{payload}}, 513 * 1024, {});
  EXPECT_THAT(result, StatusIs(StatusCode::kUnavailable, "uh oh"));

  auto const log_lines = log_backend_.ExtractLines();
  EXPECT_THAT(log_lines, ContainsOnce(HasSubstr("upload_size=" +
                                                std::to_string(513 * 1024))));
  EXPECT_THAT(log_lines, ContainsOnce(HasSubstr("UNAVAILABLE:")));
}

TEST_F(LoggingResumableUploadSessionTest, ResetSession) {
  auto mock = absl::make_unique<testing::MockResumableUploadSession>();

  EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
    return StatusOr<ResumableUploadResponse>(
        Status(AsStatus(HttpResponse{308, "uh oh", {}})));
  });

  LoggingResumableUploadSession session(std::move(mock));

  auto result = session.ResetSession();
  EXPECT_THAT(result, StatusIs(StatusCode::kFailedPrecondition, "uh oh"));

  EXPECT_THAT(log_backend_.ExtractLines(),
              ContainsOnce(HasSubstr("FAILED_PRECONDITION:")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
