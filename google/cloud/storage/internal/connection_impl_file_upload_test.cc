// Copyright 2025 Google LLC
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

#include "google/cloud/storage/internal/connection_impl.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <iterator>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::TempFile;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

class ConnectionImplFileUploadTest : public ::testing::Test {
 protected:
  static std::shared_ptr<StorageConnectionImpl> MakeConnection() {
    auto mock = std::make_unique<MockGenericStub>();
    EXPECT_CALL(*mock, options).WillRepeatedly(Return(Options{}));
    return StorageConnectionImpl::Create(std::move(mock));
  }
};

TEST_F(ConnectionImplFileUploadTest, UploadFileSimpleSuccess) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"some simple contents"};
  TempFile temp(contents);
  InsertObjectMediaRequest request("test-bucket", "test-object", "");
  auto payload =
      connection->UploadFileSimple(temp.name(), contents.size(), request);
  ASSERT_STATUS_OK(payload);
  EXPECT_EQ(*payload.value(), contents);
}

TEST_F(ConnectionImplFileUploadTest, UploadFileSimpleNonExistentFile) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  InsertObjectMediaRequest request("test-bucket", "test-object", "");
  auto payload =
      connection->UploadFileSimple("/no/such/file/exists.txt", 123, request);
  EXPECT_THAT(payload, StatusIs(StatusCode::kNotFound));
}

TEST_F(ConnectionImplFileUploadTest, UploadFileSimpleOffsetLargerThanSize) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"some simple contents"};
  TempFile temp(contents);
  InsertObjectMediaRequest request("test-bucket", "test-object", "");
  request.set_option(UploadFromOffset(contents.size() + 1));
  auto payload =
      connection->UploadFileSimple(temp.name(), contents.size(), request);
  EXPECT_THAT(payload, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ConnectionImplFileUploadTest, UploadFileSimpleWithLimit) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"0123456789"};
  TempFile temp(contents);
  InsertObjectMediaRequest request("test-bucket", "test-object", "");
  request.set_option(UploadLimit(5));
  auto payload =
      connection->UploadFileSimple(temp.name(), contents.size(), request);
  ASSERT_STATUS_OK(payload);
  EXPECT_EQ(*payload.value(), "01234");
}

TEST_F(ConnectionImplFileUploadTest, UploadFileSimpleReadError) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"some simple contents"};
  TempFile temp(contents);
  // Pass a size larger than the file to trigger a read error.
  InsertObjectMediaRequest request("test-bucket", "test-object", "");
  auto payload =
      connection->UploadFileSimple(temp.name(), contents.size() + 1, request);
  EXPECT_THAT(payload, StatusIs(StatusCode::kInternal));
}

TEST_F(ConnectionImplFileUploadTest, UploadFileResumableSuccess) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"some resumable contents"};
  TempFile temp(contents);
  ResumableUploadRequest request("test-bucket", "test-object");
  auto stream = connection->UploadFileResumable(temp.name(), request);
  ASSERT_STATUS_OK(stream);
  EXPECT_TRUE(request.HasOption<UploadContentLength>());
  EXPECT_EQ(request.GetOption<UploadContentLength>().value(), contents.size());
  std::string actual(std::istreambuf_iterator<char>(**stream), {});
  EXPECT_EQ(contents, actual);
}

TEST_F(ConnectionImplFileUploadTest, UploadFileResumableNonExistentFile) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  ResumableUploadRequest request("test-bucket", "test-object");
  auto stream =
      connection->UploadFileResumable("/no/such/file/exists.txt", request);
  EXPECT_THAT(stream, StatusIs(StatusCode::kNotFound));
}

TEST_F(ConnectionImplFileUploadTest, UploadFileResumableOffsetLargerThanSize) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"some resumable contents"};
  TempFile temp(contents);
  ResumableUploadRequest request("test-bucket", "test-object");
  request.set_option(UploadFromOffset(contents.size() + 1));
  auto stream = connection->UploadFileResumable(temp.name(), request);
  EXPECT_THAT(stream, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(ConnectionImplFileUploadTest, UploadFileResumableWithLimit) {
  std::shared_ptr<StorageConnectionImpl> connection = MakeConnection();
  std::string const contents = std::string{"0123456789"};
  TempFile temp(contents);
  ResumableUploadRequest request("test-bucket", "test-object");
  int const limit = 5;
  request.set_option(UploadLimit(limit));
  auto stream = connection->UploadFileResumable(temp.name(), request);
  ASSERT_STATUS_OK(stream);
  EXPECT_TRUE(request.HasOption<UploadContentLength>());
  EXPECT_EQ(request.GetOption<UploadContentLength>().value(), limit);

  // Read only the limited number of bytes from the stream.
  std::vector<char> buffer(limit);
  (*stream)->read(buffer.data(), buffer.size());
  std::string actual(buffer.begin(), buffer.end());

  EXPECT_EQ("01234", actual);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
