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

#include "google/cloud/storage/internal/async/object_descriptor_connection_logging.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/options.h"
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

using ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage_experimental::ObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;

Options LoggingEnabled() {
  return Options{}.set<LoggingComponentsOption>({"rpc"});
}

TEST(ObjectDescriptorConnectionLogging, Disabled) {
  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  auto actual = MakeLoggingObjectDescriptorConnection(mock, Options{});
  EXPECT_EQ(actual, mock);
}

TEST(ObjectDescriptorConnectionLogging, Enabled) {
  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  auto actual = MakeLoggingObjectDescriptorConnection(mock, LoggingEnabled());
  EXPECT_NE(actual, mock);
}

TEST(ObjectDescriptorConnectionLogging, Options) {
  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(LoggingEnabled()));
  auto actual = MakeLoggingObjectDescriptorConnection(mock, LoggingEnabled());
  EXPECT_EQ(actual->options().get<LoggingComponentsOption>(),
            LoggingEnabled().get<LoggingComponentsOption>());
}

TEST(ObjectDescriptorConnectionLogging, Read) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](ObjectDescriptorConnection::ReadParams const& p) {
        EXPECT_EQ(p.start, 100);
        EXPECT_EQ(p.length, 200);
        auto reader = std::make_unique<MockAsyncReaderConnection>();
        EXPECT_CALL(*reader, Read).WillOnce([] {
          return make_ready_future(
              AsyncReaderConnection::ReadResponse(Status{}));
        });
        return std::unique_ptr<AsyncReaderConnection>(std::move(reader));
      });
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto actual = MakeLoggingObjectDescriptorConnection(mock, LoggingEnabled());
  auto reader = actual->Read({100, 200});
  ASSERT_THAT(reader, NotNull());
  (void)reader->Read().get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ObjectDescriptorConnection::Read")));
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() <<")));
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() >> status")));
}

TEST(ObjectDescriptorConnectionLogging, Metadata) {
  ScopedLog log;

  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, metadata).WillOnce([] {
    return google::storage::v2::Object{};
  });

  auto actual = MakeLoggingObjectDescriptorConnection(mock, LoggingEnabled());
  (void)actual->metadata();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Not(Contains(HasSubstr("metadata"))));
}

TEST(ObjectDescriptorConnectionLogging, MakeSubsequentStream) {
  ScopedLog log;

  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, MakeSubsequentStream).WillOnce([] { return; });

  auto actual = MakeLoggingObjectDescriptorConnection(mock, LoggingEnabled());
  actual->MakeSubsequentStream();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Not(Contains(HasSubstr("MakeSubsequentStream"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
