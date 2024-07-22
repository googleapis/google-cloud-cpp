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

#include "google/cloud/storage/async/reader.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;

TEST(AsyncReader, Basic) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        return make_ready_future(ReadResponse(ReadPayload("test-message-1")));
      })
      .WillOnce([] {
        return make_ready_future(ReadResponse(ReadPayload("test-message-2")));
      })
      .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));

  ReadPayload payload;
  auto token = storage_internal::MakeAsyncToken(mock.get());
  AsyncReader reader(std::move(mock));

  auto p = reader.Read(std::move(token)).get();
  ASSERT_STATUS_OK(p);
  std::tie(payload, token) = *std::move(p);
  ASSERT_TRUE(token.valid());
  EXPECT_THAT(payload.contents(), ElementsAre("test-message-1"));

  p = reader.Read(std::move(token)).get();
  ASSERT_STATUS_OK(p);
  std::tie(payload, token) = *std::move(p);
  ASSERT_TRUE(token.valid());
  EXPECT_THAT(payload.contents(), ElementsAre("test-message-2"));

  EXPECT_THAT(reader.GetRequestMetadata().headers, IsEmpty());

  p = reader.Read(std::move(token)).get();
  ASSERT_STATUS_OK(p);
  std::tie(payload, token) = *std::move(p);
  ASSERT_FALSE(token.valid());

  auto const metadata = reader.GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

TEST(AsyncReader, ErrorDuringRead) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        return make_ready_future(ReadResponse(ReadPayload("test-message-1")));
      })
      .WillOnce(
          [] { return make_ready_future(ReadResponse(PermanentError())); });

  ReadPayload payload;
  auto token = storage_internal::MakeAsyncToken(mock.get());
  AsyncReader reader(std::move(mock));

  auto p = reader.Read(std::move(token)).get();
  ASSERT_STATUS_OK(p);
  std::tie(payload, token) = *std::move(p);
  ASSERT_TRUE(token.valid());
  EXPECT_THAT(payload.contents(), ElementsAre("test-message-1"));

  p = reader.Read(std::move(token)).get();
  EXPECT_THAT(p, StatusIs(PermanentError().code()));
}

TEST(AsyncReader, Discard) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read/1").then(
        [](auto) { return ReadResponse(ReadPayload("test-message-1")); });
  });
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read/2").then(
        [](auto) { return ReadResponse(ReadPayload("test-message-2")); });
  });
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read/3").then(
        [](auto) { return ReadResponse(Status{}); });
  });

  ReadPayload payload;
  auto token = storage_internal::MakeAsyncToken(mock.get());

  {
    AsyncReader reader(std::move(mock));
    auto pending = reader.Read(std::move(token));
    auto s = sequencer.PopFrontWithName();
    EXPECT_THAT(s.second, "Read/1");
    s.first.set_value(true);
    auto p = pending.get();
    ASSERT_STATUS_OK(p);
    std::tie(payload, token) = *std::move(p);
    ASSERT_TRUE(token.valid());
    EXPECT_THAT(payload.contents(), ElementsAre("test-message-1"));
  }
  // The destructor should setup a background task to cancel the streaming RPC
  // and discard any data until the RPC finishes.
  auto s = sequencer.PopFrontWithName();
  EXPECT_THAT(s.second, "Read/2");
  s.first.set_value(true);

  s = sequencer.PopFrontWithName();
  EXPECT_THAT(s.second, "Read/3");
  s.first.set_value(false);
}

TEST(AsyncReader, ErrorOnClosedStream) {
  AsyncReader reader;
  auto const actual =
      reader.Read(storage_internal::MakeAsyncToken(&reader)).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
}

TEST(AsyncReader, ErrorWithInvalidToken) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(ReadResponse(Status{}));
  });

  AsyncReader reader(std::move(mock));
  auto const actual = reader.Read(AsyncToken()).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST(AsyncReader, ErrorWithMismatchedToken) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(ReadResponse(Status{}));
  });

  int placeholder = 0;
  AsyncReader reader(std::move(mock));
  auto const actual =
      reader.Read(storage_internal::MakeAsyncToken(&placeholder)).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
