// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <iostream>
#include <string>

namespace {

using ::testing::Return;

//! [mock successful readobject]
TEST(StorageMockingSamples, MockReadObject) {
  namespace gcs = ::google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  auto client = gcs::testing::ClientFromMock(mock);

  std::string const text = "this is a mock http response";
  std::size_t offset = 0;
  // Simulate a Read() call in the MockObjectReadSource object created below
  auto simulate_read = [&text, &offset](void* buf, std::size_t n) {
    auto const l = (std::min)(n, text.size() - offset);
    std::memcpy(buf, text.data() + offset, l);
    offset += l;
    return gcs::internal::ReadSourceResult{
        l, gcs::internal::HttpResponse{200, {}, {}}};
  };
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([&](gcs::internal::ReadObjectRangeRequest const& request) {
        EXPECT_EQ(request.bucket_name(), "mock-bucket-name") << request;
        std::unique_ptr<gcs::testing::MockObjectReadSource> mock_source(
            new gcs::testing::MockObjectReadSource);
        ::testing::InSequence seq;
        EXPECT_CALL(*mock_source, IsOpen()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_source, Read).WillOnce(simulate_read);
        EXPECT_CALL(*mock_source, IsOpen()).WillRepeatedly(Return(false));

        return google::cloud::make_status_or(
            std::unique_ptr<gcs::internal::ObjectReadSource>(
                std::move(mock_source)));
      });

  gcs::ObjectReadStream stream =
      client.ReadObject("mock-bucket-name", "mock-object-name");

  // Reading the payload of http response stored in stream
  std::string actual{std::istreambuf_iterator<char>(stream), {}};
  EXPECT_EQ(actual, text);
  stream.Close();
}
//! [mock successful readobject]

//! [mock successful writeobject]
TEST(StorageMockingSamples, MockWriteObject) {
  namespace gcs = ::google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  auto client = gcs::testing::ClientFromMock(mock);

  gcs::ObjectMetadata expected_metadata;

  using gcs::internal::CreateResumableUploadResponse;
  using gcs::internal::QueryResumableUploadResponse;
  EXPECT_CALL(*mock, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"test-only-upload-id"}));
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{/*committed_size=*/absl::nullopt,
                                       /*object_metadata=*/expected_metadata}));

  auto stream = client.WriteObject("mock-bucket-name", "mock-object-name");
  stream << "Hello World!";
  stream.Close();
  //! [mock successful writeobject]
}

//! [mock failed readobject]
TEST(StorageMockingSamples, MockReadObjectFailure) {
  namespace gcs = ::google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  auto client = gcs::testing::ClientFromMock(mock);

  std::string text = "this is a mock http response";
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([](gcs::internal::ReadObjectRangeRequest const& request) {
        EXPECT_EQ(request.bucket_name(), "mock-bucket-name") << request;
        auto* mock_source = new gcs::testing::MockObjectReadSource;
        ::testing::InSequence seq;
        EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_source, Read)
            .WillOnce(Return(google::cloud::Status(
                google::cloud::StatusCode::kInvalidArgument,
                "Invalid Argument")));
        EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));

        std::unique_ptr<gcs::internal::ObjectReadSource> result(mock_source);

        return google::cloud::make_status_or(std::move(result));
      });

  gcs::ObjectReadStream stream =
      client.ReadObject("mock-bucket-name", "mock-object-name");
  EXPECT_TRUE(stream.bad());
  stream.Close();
}
//! [mock failed readobject]

//! [mock failed writeobject]
TEST(StorageMockingSamples, MockWriteObjectFailure) {
  namespace gcs = ::google::cloud::storage;

  std::shared_ptr<gcs::testing::MockClient> mock =
      std::make_shared<gcs::testing::MockClient>();
  auto client = gcs::testing::ClientFromMock(mock);

  using gcs::internal::CreateResumableUploadResponse;
  using gcs::internal::QueryResumableUploadResponse;
  EXPECT_CALL(*mock, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"test-only-upload-id"}));
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(google::cloud::Status{
          google::cloud::StatusCode::kInvalidArgument, "Invalid Argument"}));

  auto stream = client.WriteObject("mock-bucket-name", "mock-object-name");
  stream << "Hello World!";
  stream.Close();

  EXPECT_TRUE(stream.bad());
}
//! [mock failed writeobject]

}  // anonymous namespace
