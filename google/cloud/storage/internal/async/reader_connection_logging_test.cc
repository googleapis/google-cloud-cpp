// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/reader_connection_logging.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage_experimental::ReadPayload;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;

Options LoggingEnabled() {
  return Options{}.set<LoggingComponentsOption>({"rpc"});
}

TEST(ReaderConnectionLogging, Disabled) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  auto* mock_ptr = mock.get();
  auto actual = MakeLoggingReaderConnection(Options{}, std::move(mock));
  EXPECT_EQ(actual.get(), mock_ptr);
}

TEST(ReaderConnectionLogging, Enabled) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  auto* mock_ptr = mock.get();
  auto actual = MakeLoggingReaderConnection(LoggingEnabled(), std::move(mock));
  EXPECT_NE(actual.get(), mock_ptr);
}

TEST(ReaderConnectionLogging, ReadSuccess) {
  ScopedLog log;

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(AsyncReaderConnection::ReadResponse(
        ReadPayload("test-payload").set_offset(123)));
  });

  auto actual = MakeLoggingReaderConnection(LoggingEnabled(), std::move(mock));
  (void)actual->Read().get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() <<")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReaderConnectionLogging::Read() >>"
                                            " payload.size=12, offset=123")));
}

TEST(ReaderConnectionLogging, ReadError) {
  ScopedLog log;

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(
        AsyncReaderConnection::ReadResponse(PermanentError()));
  });

  auto actual = MakeLoggingReaderConnection(LoggingEnabled(), std::move(mock));
  auto result = actual->Read().get();
  ASSERT_TRUE(absl::holds_alternative<Status>(result));
  EXPECT_THAT(absl::get<Status>(result), StatusIs(PermanentError().code()));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() <<")));
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() >> status=" +
                                 PermanentError().message())));
}

TEST(ReaderConnectionLogging, Cancel) {
  ScopedLog log;

  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto actual = MakeLoggingReaderConnection(LoggingEnabled(), std::move(mock));
  actual->Cancel();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Cancel()")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
