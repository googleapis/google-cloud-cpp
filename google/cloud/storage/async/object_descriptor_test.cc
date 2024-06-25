// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/object_descriptor.h"
#include "google/cloud/storage/mocks/mock_async_object_descriptor_connection.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage_mocks::MockAsyncObjectDescriptorConnection;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::testing::ElementsAre;
using ::testing::Return;
using ReadParams = ::google::cloud::storage_experimental::
    ObjectDescriptorConnection::ReadParams;
using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;

TEST(ObjectDescriptor, Basic) {
  auto mock = std::make_shared<MockAsyncObjectDescriptorConnection>();
  EXPECT_CALL(*mock, metadata).WillOnce(Return(absl::nullopt));
  EXPECT_CALL(*mock, Read)
      .WillOnce([](ReadParams p) -> std::unique_ptr<AsyncReaderConnection> {
        EXPECT_EQ(p.start, 100);
        EXPECT_EQ(p.limit, 200);
        auto reader = std::make_unique<MockAsyncReaderConnection>();
        EXPECT_CALL(*reader, Read)
            .WillOnce([] {
              return make_ready_future(ReadResponse(ReadPayload(
                  std::string("The quick brown fox jumps over the lazy dog"))));
            })
            .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });
        return reader;
      });

  auto tested = ObjectDescriptor(mock);
  EXPECT_FALSE(tested.metadata().has_value());
  AsyncReader reader;
  AsyncToken token;
  std::tie(reader, token) = tested.Read(100, 200);
  ASSERT_TRUE(token.valid());

  auto r1 = reader.Read(std::move(token)).get();
  ASSERT_STATUS_OK(r1);
  ReadPayload payload;
  std::tie(payload, token) = *std::move(r1);
  EXPECT_THAT(payload.contents(),
              ElementsAre("The quick brown fox jumps over the lazy dog"));

  auto r2 = reader.Read(std::move(token)).get();
  ASSERT_STATUS_OK(r2);
  std::tie(payload, token) = *std::move(r2);
  EXPECT_FALSE(token.valid());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
