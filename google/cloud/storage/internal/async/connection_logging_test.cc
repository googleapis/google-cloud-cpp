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

#include "google/cloud/storage/internal/async/connection_logging.h"
#include "google/cloud/storage/async/object_descriptor_connection.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/async/object_descriptor_connection_logging.h"
#include "google/cloud/storage/internal/async/reader_connection_logging.h"
#include "google/cloud/storage/mocks/mock_async_connection.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/mocks/mock_async_rewriter_connection.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
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
using ::google::cloud::storage_experimental::AsyncWriterConnection;
using ::google::cloud::storage_mocks::MockAsyncConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::storage_mocks::MockAsyncRewriterConnection;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Return;

Options LoggingEnabled() {
  return Options{}.set<LoggingComponentsOption>({"rpc"});
}

TEST(ConnectionLogging, Disabled) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(Options{}));
  auto actual = MakeLoggingAsyncConnection(mock);
  EXPECT_EQ(actual.get(), mock.get());
}

TEST(ConnectionLogging, Enabled) {
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(LoggingEnabled()));
  auto actual = MakeLoggingAsyncConnection(mock);
  EXPECT_NE(actual.get(), mock.get());
}

TEST(ConnectionLogging, ReadObjectSuccess) {
  ScopedLog log;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, ReadObject).WillOnce([] {
    auto reader = std::make_unique<MockAsyncReaderConnection>();
    EXPECT_CALL(*reader, Read)
        .WillOnce(Return(
            make_ready_future(AsyncReaderConnection::ReadResponse(Status{}))));
    return make_ready_future(
        StatusOr<std::unique_ptr<AsyncReaderConnection>>(std::move(reader)));
  });
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto reader = conn->ReadObject({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(reader);
  (void)(*reader)->Read().get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObject(bucket=, object=)")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObject succeeded")));
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() <<")));
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReaderConnectionLogging::Read() >> status")));
}

TEST(ConnectionLogging, ReadObjectError) {
  ScopedLog log;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, ReadObject).WillOnce([] {
    return make_ready_future(
        StatusOr<std::unique_ptr<AsyncReaderConnection>>(PermanentError()));
  });
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto reader = conn->ReadObject({{}, LoggingEnabled()}).get();
  EXPECT_THAT(reader, StatusIs(PermanentError().code()));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObject(bucket=, object=)")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObject failed")));
}

TEST(ConnectionLogging, ReadObjectRangeSuccess) {
  ScopedLog log;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, ReadObjectRange)
      .WillOnce(Return(make_ready_future(
          make_status_or(storage_experimental::ReadPayload{}))));
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto conn = MakeLoggingAsyncConnection(mock);
  (void)conn->ReadObjectRange({}).get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReadObjectRange(bucket=, object=)")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObjectRange succeeded")));
}

TEST(ConnectionLogging, ReadObjectRangeError) {
  ScopedLog log;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, ReadObjectRange)
      .WillOnce(Return(make_ready_future(
          StatusOr<storage_experimental::ReadPayload>(PermanentError()))));
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto conn = MakeLoggingAsyncConnection(mock);
  (void)conn->ReadObjectRange({}).get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReadObjectRange(bucket=, object=)")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObjectRange failed")));
}

TEST(ConnectionLogging, ReadObjectRangeNotReady) {
  ScopedLog log;
  promise<StatusOr<storage_experimental::ReadPayload>> p;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, ReadObjectRange).WillOnce(Return(p.get_future()));
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto fut = conn->ReadObjectRange({});
  EXPECT_FALSE(fut.is_ready());
  p.set_value(storage_experimental::ReadPayload{});
  (void)fut.get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReadObjectRange(bucket=, object=)")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObjectRange succeeded")));
}

TEST(ConnectionLogging, ReadObjectRangeNotReadyWithError) {
  ScopedLog log;
  promise<StatusOr<storage_experimental::ReadPayload>> p;

  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, ReadObjectRange).WillOnce(Return(p.get_future()));
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto fut = conn->ReadObjectRange({});
  EXPECT_FALSE(fut.is_ready());
  p.set_value(PermanentError());
  (void)fut.get();

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines,
              Contains(HasSubstr("ReadObjectRange(bucket=, object=)")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ReadObjectRange failed")));
}

TEST(ConnectionLogging, StartAppendableObjectUpload) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, StartAppendableObjectUpload).WillOnce([] {
    return make_ready_future(StatusOr<std::unique_ptr<AsyncWriterConnection>>(
        std::make_unique<MockAsyncWriterConnection>()));
  });

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->StartAppendableObjectUpload({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, ResumeAppendableObjectUpload) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, ResumeAppendableObjectUpload).WillOnce([] {
    return make_ready_future(StatusOr<std::unique_ptr<AsyncWriterConnection>>(
        std::make_unique<MockAsyncWriterConnection>()));
  });

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result =
      conn->ResumeAppendableObjectUpload({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, StartUnbufferedUpload) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, StartUnbufferedUpload).WillOnce([] {
    return make_ready_future(StatusOr<std::unique_ptr<AsyncWriterConnection>>(
        std::make_unique<MockAsyncWriterConnection>()));
  });

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->StartUnbufferedUpload({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, StartBufferedUpload) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, StartBufferedUpload).WillOnce([] {
    return make_ready_future(StatusOr<std::unique_ptr<AsyncWriterConnection>>(
        std::make_unique<MockAsyncWriterConnection>()));
  });

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->StartBufferedUpload({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, ResumeUnbufferedUpload) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, ResumeUnbufferedUpload).WillOnce([] {
    return make_ready_future(StatusOr<std::unique_ptr<AsyncWriterConnection>>(
        std::make_unique<MockAsyncWriterConnection>()));
  });

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->ResumeUnbufferedUpload({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, ResumeBufferedUpload) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, ResumeBufferedUpload).WillOnce([] {
    return make_ready_future(StatusOr<std::unique_ptr<AsyncWriterConnection>>(
        std::make_unique<MockAsyncWriterConnection>()));
  });

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->ResumeBufferedUpload({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, ComposeObject) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(
          make_ready_future(make_status_or(google::storage::v2::Object{}))));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->ComposeObject({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, DeleteObject) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(make_ready_future(Status{})));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->DeleteObject({{}, LoggingEnabled()}).get();
  ASSERT_STATUS_OK(result);
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

TEST(ConnectionLogging, RewriteObject) {
  ScopedLog log;
  auto mock = std::make_shared<MockAsyncConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(LoggingEnabled()));
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce(Return(std::make_shared<MockAsyncRewriterConnection>()));

  auto conn = MakeLoggingAsyncConnection(mock);
  auto result = conn->RewriteObject({{}, LoggingEnabled()});
  ASSERT_THAT(result, NotNull());
  EXPECT_THAT(log.ExtractLines(), IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
