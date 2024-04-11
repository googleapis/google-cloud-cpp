// Copyright 2023 Google LLC
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

#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/mocks/mock_async_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include <gmock/gmock.h>
#include <iostream>
#include <string>

namespace {

using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Return;

//! [mock-async-delete-object]
/// Shows how to mock simple APIs, including `DeleteObject` and `ComposeObject`.
TEST(StorageAsyncMockingSamples, MockDeleteObject) {
  namespace gc = ::google::cloud;
  namespace gcs_ex = ::google::cloud::storage_experimental;
  auto mock =
      std::make_shared<google::cloud::storage_mocks::MockAsyncConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(ByMove(gc::make_ready_future(gc::Status{}))));

  auto client = gcs_ex::AsyncClient(mock);
  auto actual =
      client.DeleteObject(gcs_ex::BucketName("test-bucket"), "test-object")
          .get();
  EXPECT_TRUE(actual.ok());
}
//! [mock-async-delete-object]

//! [mock-async-read-object]
/// Shows how to mock more complex APIs, such as `ReadObject()`.
TEST(StorageAsyncMockingSamples, MockReadObject) {
  namespace gc = ::google::cloud;
  namespace gcs_ex = ::google::cloud::storage_experimental;
  auto mock =
      std::make_shared<google::cloud::storage_mocks::MockAsyncConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadObject).WillOnce([] {
    using ReadResponse = gcs_ex::AsyncReaderConnection::ReadResponse;
    using ReadPayload = gcs_ex::ReadPayload;
    auto reader = std::make_unique<
        google::cloud::storage_mocks::MockAsyncReaderConnection>();
    EXPECT_CALL(*reader, Read)
        .WillOnce([] {
          // Return a payload object. In this test we just include some data.
          // More complex tests may include additional information such as
          // object metadata.
          return gc::make_ready_future(
              ReadResponse(ReadPayload(std::string("test-contents"))));
        })
        .WillOnce([] {
          // To terminate the reader, return a `gc::Status`. In this test
          // we finish the stream with a successful status.
          return gc::make_ready_future(ReadResponse(gc::Status{}));
        });
    return gc::make_ready_future(gc::make_status_or(
        std::unique_ptr<gcs_ex::AsyncReaderConnection>(std::move(reader))));
  });

  auto client = gcs_ex::AsyncClient(mock);
  // To simplify the test we use .get() to "block" until the gc::future is
  // ready, and then use `.value()` to get the `StatusOr<>` values or an
  // exception.
  gcs_ex::AsyncReader reader;
  gcs_ex::AsyncToken token;
  std::tie(reader, token) =
      client.ReadObject(gcs_ex::BucketName("test-bucket"), "test-object")
          .get()
          .value();

  gcs_ex::ReadPayload payload;
  gcs_ex::AsyncToken t;  // Avoid use-after-move warnings from clang-tidy.
  std::tie(payload, t) = reader.Read(std::move(token)).get().value();
  token = std::move(t);
  EXPECT_THAT(payload.contents(), ElementsAre("test-contents"));
  ASSERT_TRUE(token.valid());

  std::tie(payload, t) = reader.Read(std::move(token)).get().value();
  token = std::move(t);
  EXPECT_FALSE(token.valid());
}
//! [mock-async-read-object]

}  // namespace
