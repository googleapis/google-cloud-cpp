// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace storage_proto = ::google::storage::v1;

class MockStream : public google::cloud::internal::StreamingReadRpc<
                       storage_proto::GetObjectMediaResponse> {
 public:
  using ReadReturn =
      absl::variant<Status, storage_proto::GetObjectMediaResponse>;
  MOCK_METHOD(ReadReturn, Read, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(google::cloud::internal::StreamingRpcMetadata, GetRequestMetadata,
              (), (const, override));
};

TEST(GrpcObjectReadSource, Simple) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content("0123456789");
        return response;
      })
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content(
            " The quick brown fox jumps over the lazy dog");
        return response;
      })
      .WillOnce(Return(Status{}));
  GrpcObjectReadSource tested(std::move(mock));
  std::string expected =
      "0123456789 The quick brown fox jumps over the lazy dog";
  std::vector<char> buffer(1024);
  auto response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(expected.size(), response->bytes_received);
  EXPECT_EQ(100, response->response.status_code);
  std::string actual(buffer.data(), expected.size());
  EXPECT_EQ(expected, actual);

  auto status = tested.Close();
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(200, status->status_code);
}

TEST(GrpcObjectReadSource, EmptyWithError) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  GrpcObjectReadSource tested(std::move(mock));
  std::vector<char> buffer(1024);
  EXPECT_THAT(tested.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));

  EXPECT_THAT(tested.Close(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(GrpcObjectReadSource, DataWithError) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content("0123456789");
        return response;
      })
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  GrpcObjectReadSource tested(std::move(mock));
  std::string expected = "0123456789";
  std::vector<char> buffer(1024);
  auto response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(expected.size(), response->bytes_received);
  std::string actual(buffer.data(), response->bytes_received);
  EXPECT_EQ(expected, actual);

  EXPECT_THAT(tested.Read(buffer.data(), buffer.size()),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));

  EXPECT_THAT(tested.Close(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(GrpcObjectReadSource, UseSpillBuffer) {
  auto mock = absl::make_unique<MockStream>();
  auto const trailer_size = 1024;
  std::string const expected_1 = "0123456789";
  std::string const expected_2(trailer_size, 'A');
  ASSERT_LT(expected_1.size(), expected_2.size());
  std::string const contents = expected_1 + expected_2;
  EXPECT_CALL(*mock, Read)
      .WillOnce([&contents]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content(contents);
        return response;
      })
      .WillOnce(Return(Status{}));
  GrpcObjectReadSource tested(std::move(mock));
  std::vector<char> buffer(trailer_size);
  auto response = tested.Read(buffer.data(), expected_1.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(expected_1.size(), response->bytes_received);
  std::string actual(buffer.data(), response->bytes_received);
  EXPECT_EQ(expected_1, actual);

  response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(expected_2.size(), response->bytes_received);
  actual = std::string(buffer.data(), response->bytes_received);
  EXPECT_EQ(expected_2, actual);

  response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(0, response->bytes_received);

  auto status = tested.Close();
  EXPECT_STATUS_OK(status);
}

TEST(GrpcObjectReadSource, UseSpillBufferMany) {
  auto mock = absl::make_unique<MockStream>();
  std::string const contents = "0123456789";
  EXPECT_CALL(*mock, Read)
      .WillOnce([&contents]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content(contents);
        return response;
      })
      .WillOnce(Return(Status{}));
  GrpcObjectReadSource tested(std::move(mock));
  std::vector<char> buffer(1024);
  auto response = tested.Read(buffer.data(), 3);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(3, response->bytes_received);
  auto actual = std::string(buffer.data(), response->bytes_received);
  EXPECT_EQ("012", actual);

  response = tested.Read(buffer.data(), 4);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(4, response->bytes_received);
  actual = std::string(buffer.data(), response->bytes_received);
  EXPECT_EQ("3456", actual);

  response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(3, response->bytes_received);
  actual = std::string(buffer.data(), response->bytes_received);
  EXPECT_EQ("789", actual);

  response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(0, response->bytes_received);

  auto status = tested.Close();
  EXPECT_STATUS_OK(status);
}

TEST(GrpcObjectReadSource, PreserveChecksums) {
  auto mock = absl::make_unique<MockStream>();
  std::string const expected_md5 = "nhB9nTcrtoJr2B01QqQZ1g==";
  std::string const expected_crc32c = "ImIEBA==";
  EXPECT_CALL(*mock, Read)
      .WillOnce([&]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content("The quick brown");
        response.mutable_object_checksums()->set_md5_hash(
            GrpcClient::MD5ToProto(expected_md5));
        response.mutable_object_checksums()->mutable_crc32c()->set_value(
            GrpcClient::Crc32cToProto(expected_crc32c));
        return response;
      })
      .WillOnce([&] {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content(
            " fox jumps over the lazy dog");
        // The headers may be included more than once in the stream,
        // `GrpcObjectReadSource` should return them only once.
        response.mutable_object_checksums()->set_md5_hash(
            GrpcClient::MD5ToProto(expected_md5));
        response.mutable_object_checksums()->mutable_crc32c()->set_value(
            GrpcClient::Crc32cToProto(expected_crc32c));
        return response;
      })
      .WillOnce(Return(Status{}));
  GrpcObjectReadSource tested(std::move(mock));
  std::string const expected = "The quick brown fox jumps over the lazy dog";
  std::vector<char> buffer(1024);
  auto response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(expected.size(), response->bytes_received);
  EXPECT_EQ(100, response->response.status_code);
  std::string actual(buffer.data(), expected.size());
  EXPECT_EQ(expected, actual);
  auto const& headers = response->response.headers;
  EXPECT_FALSE(headers.find("x-goog-hash") == headers.end());
  auto const values = [&headers] {
    std::vector<std::string> v;
    for (auto const& kv : headers) {
      if (kv.first != "x-goog-hash") continue;
      v.push_back(kv.second);
    }
    return v;
  }();
  EXPECT_THAT(values, UnorderedElementsAre("crc32c=" + expected_crc32c,
                                           "md5=" + expected_md5));

  auto status = tested.Close();
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(200, status->status_code);
}

TEST(GrpcObjectReadSource, HandleEmptyResponses) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] { return storage_proto::GetObjectMediaResponse{}; })
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content("The quick brown ");
        return response;
      })
      .WillOnce([] { return storage_proto::GetObjectMediaResponse{}; })
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content("fox jumps over ");
        return response;
      })
      .WillOnce([] { return storage_proto::GetObjectMediaResponse{}; })
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content("the lazy dog");
        return response;
      })
      .WillOnce(Return(Status{}));
  GrpcObjectReadSource tested(std::move(mock));
  std::string const expected = "The quick brown fox jumps over the lazy dog";
  std::vector<char> buffer(1024);
  auto response = tested.Read(buffer.data(), buffer.size());
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(expected.size(), response->bytes_received);
  EXPECT_EQ(100, response->response.status_code);
  std::string actual(buffer.data(), expected.size());
  EXPECT_EQ(expected, actual);

  auto status = tested.Close();
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(200, status->status_code);
}

TEST(GrpcObjectReadSource, HandleExtraRead) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([]() {
        storage_proto::GetObjectMediaResponse response;
        response.mutable_checksummed_data()->set_content(
            "The quick brown fox jumps over the lazy dog");
        return response;
      })
      .WillOnce(Return(Status{}));
  GrpcObjectReadSource tested(std::move(mock));
  std::string const expected = "The quick brown fox jumps over the lazy dog";
  std::vector<char> buffer(1024);
  auto response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(expected.size(), response->bytes_received);
  EXPECT_EQ(100, response->response.status_code);
  std::string actual(buffer.data(), expected.size());
  EXPECT_EQ(expected, actual);

  response = tested.Read(buffer.data(), buffer.size());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(0, response->bytes_received);
  EXPECT_EQ(200, response->response.status_code);

  auto status = tested.Close();
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(200, status->status_code);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
