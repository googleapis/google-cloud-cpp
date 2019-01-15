// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/logging_resumable_upload_session.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/log.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::google::cloud::testing_util::CaptureLogLinesBackend;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;

class LoggingResumableUploadSessionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend = std::make_shared<CaptureLogLinesBackend>();
    log_backend_id = google::cloud::LogSink::Instance().AddBackend(log_backend);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id);
    log_backend_id = 0;
    log_backend.reset();
  }

  std::size_t CountLines(std::string const& substr) {
    return std::count_if(log_backend->log_lines.begin(),
                         log_backend->log_lines.end(),
                         [substr](std::string const& line) {
                           return std::string::npos != line.find(substr);
                         });
  }

  std::shared_ptr<CaptureLogLinesBackend> log_backend = nullptr;
  long log_backend_id = 0;
};

TEST_F(LoggingResumableUploadSessionTest, UploadChunk) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  std::string const payload = "test-payload-data";
  EXPECT_CALL(*mock, UploadChunk(_, _))
      .WillOnce(Invoke([&](std::string const& p, std::uint64_t s) {
        EXPECT_EQ(payload, p);
        EXPECT_EQ(513 * 1024, s);
        return StatusOr<ResumableUploadResponse>(
            AsStatus(HttpResponse{503, "uh oh", {}}));
      }));

  LoggingResumableUploadSession session(std::move(mock));

  auto result = session.UploadChunk(payload, 513 * 1024);
  EXPECT_EQ(StatusCode::kUnavailable, result.status().code());
  EXPECT_EQ("uh oh", result.status().message());

  EXPECT_EQ(1U, CountLines("upload_size=" + std::to_string(513 * 1024UL)));
  EXPECT_EQ(1U, CountLines("[UNAVAILABLE]"));
}

TEST_F(LoggingResumableUploadSessionTest, ResetSession) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  EXPECT_CALL(*mock, ResetSession()).WillOnce(Invoke([&]() {
    return StatusOr<ResumableUploadResponse>(
        Status(AsStatus(HttpResponse{308, "uh oh", {}})));
  }));

  LoggingResumableUploadSession session(std::move(mock));

  auto result = session.ResetSession();
  EXPECT_EQ(StatusCode::kFailedPrecondition, result.status().code());
  EXPECT_EQ("uh oh", result.status().message());

  EXPECT_EQ(1U, CountLines("[FAILED_PRECONDITION]"));
}

TEST_F(LoggingResumableUploadSessionTest, NextExpectedByte) {
  auto mock = google::cloud::internal::make_unique<
      testing::MockResumableUploadSession>();

  EXPECT_CALL(*mock, next_expected_byte()).WillOnce(Invoke([&]() {
    return 512 * 1024U;
  }));

  LoggingResumableUploadSession session(std::move(mock));

  auto result = session.next_expected_byte();
  EXPECT_EQ(512 * 1024, result);

  EXPECT_EQ(1U, CountLines(std::to_string(512 * 1024)));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
