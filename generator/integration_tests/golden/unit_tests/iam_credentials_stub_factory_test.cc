// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/internal/iam_credentials_stub_factory.gcpcxx.pb.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

using ::testing::HasSubstr;

class IAMCredentialsStubFactoryTest : public ::testing::Test {
protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::vector<std::string> ClearLogLines() { return backend_->ClearLogLines(); }

private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0; // NOLINT
};

TEST_F(IAMCredentialsStubFactoryTest, DefaultStubWithoutLogging) {
  auto default_stub = CreateDefaultIAMCredentialsStub({});
  auto const log_lines = ClearLogLines();
  EXPECT_EQ(log_lines.size(), 0);
}

TEST_F(IAMCredentialsStubFactoryTest, DefaultStubWithLogging) {
  golden::IAMCredentialsConnectionOptions options;
  options.enable_tracing("rpc");
  auto default_stub = CreateDefaultIAMCredentialsStub(options);
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Enabled logging for gRPC calls")));
}

} // namespace
} // namespace golden_internal
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
