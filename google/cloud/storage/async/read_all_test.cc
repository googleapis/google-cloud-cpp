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

#include "google/cloud/storage/async/read_all.h"
#include "google/cloud/storage/mocks/mock_async_reader_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_mocks::MockAsyncReaderConnection;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;

using ReadResponse =
    ::google::cloud::storage_experimental::AsyncReaderConnection::ReadResponse;

TEST(ReadAll, Basic) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        auto object = google::storage::v2::Object{};
        object.set_name("test-only-name");
        return make_ready_future(ReadResponse(
            ReadPayload("test-message-1").set_metadata(std::move(object))));
      })
      .WillOnce([] {
        return make_ready_future(ReadResponse(ReadPayload("test-message-2")));
      })
      .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  auto payload = ReadAll(AsyncReader(std::move(mock)), std::move(token)).get();
  ASSERT_STATUS_OK(payload);
  auto expected_object = google::storage::v2::Object{};
  expected_object.set_name("test-only-name");
  EXPECT_THAT(payload->metadata(), Optional(IsProtoEqual(expected_object)));
  EXPECT_THAT(payload->contents(),
              ElementsAre("test-message-1", "test-message-2"));
}

TEST(ReadAll, BasicFromStatusOr) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        auto object = google::storage::v2::Object{};
        object.set_name("test-only-name");
        return make_ready_future(ReadResponse(
            ReadPayload("test-message-1").set_metadata(std::move(object))));
      })
      .WillOnce([] {
        return make_ready_future(ReadResponse(ReadPayload("test-message-2")));
      })
      .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  auto payload = ReadAll(make_status_or(std::make_pair(
                             AsyncReader(std::move(mock)), std::move(token))))
                     .get();
  ASSERT_STATUS_OK(payload);
  auto expected_object = google::storage::v2::Object{};
  expected_object.set_name("test-only-name");
  EXPECT_THAT(payload->metadata(), Optional(IsProtoEqual(expected_object)));
  EXPECT_THAT(payload->contents(),
              ElementsAre("test-message-1", "test-message-2"));
}

TEST(ReadAll, BasicFromStatusOrWithError) {
  auto payload =
      ReadAll(StatusOr<std::pair<AsyncReader, AsyncToken>>(PermanentError()))
          .get();
  EXPECT_THAT(payload, StatusIs(PermanentError().code()));
}

TEST(ReadAll, BasicFromFuture) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        auto object = google::storage::v2::Object{};
        object.set_name("test-only-name");
        return make_ready_future(ReadResponse(
            ReadPayload("test-message-1").set_metadata(std::move(object))));
      })
      .WillOnce([] {
        return make_ready_future(ReadResponse(ReadPayload("test-message-2")));
      })
      .WillOnce([] { return make_ready_future(ReadResponse(Status{})); });

  promise<void> p;
  auto pending =
      ReadAll(p.get_future().then([m = std::move(mock)](auto) mutable {
        auto token = storage_internal::MakeAsyncToken(m.get());
        return make_status_or(
            std::make_pair(AsyncReader(std::move(m)), std::move(token)));
      }));
  ASSERT_FALSE(pending.is_ready());
  p.set_value();
  auto payload = pending.get();
  ASSERT_STATUS_OK(payload);
  auto expected_object = google::storage::v2::Object{};
  expected_object.set_name("test-only-name");
  EXPECT_THAT(payload->metadata(), Optional(IsProtoEqual(expected_object)));
  EXPECT_THAT(payload->contents(),
              ElementsAre("test-message-1", "test-message-2"));
}

TEST(ReadAll, BasicFromFutureWithError) {
  promise<void> p;
  auto payload = ReadAll(p.get_future().then([](auto) {
    return StatusOr<std::pair<AsyncReader, AsyncToken>>(PermanentError());
  }));
  EXPECT_FALSE(payload.is_ready());
  p.set_value();
  EXPECT_THAT(payload.get(), StatusIs(PermanentError().code()));
}

TEST(ReadAll, Empty) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(ReadResponse(Status{}));
  });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  auto payload = ReadAll(AsyncReader(std::move(mock)), std::move(token)).get();
  ASSERT_STATUS_OK(payload);
  EXPECT_THAT(payload->contents(), IsEmpty());
}

TEST(ReadAll, Error) {
  auto mock = std::make_unique<MockAsyncReaderConnection>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([] {
        auto object = google::storage::v2::Object{};
        object.set_name("test-only-name");
        return make_ready_future(ReadResponse(
            ReadPayload("test-message-1").set_metadata(std::move(object))));
      })
      .WillOnce(
          [] { return make_ready_future(ReadResponse(PermanentError())); });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  auto payload = ReadAll(AsyncReader(std::move(mock)), std::move(token)).get();
  EXPECT_THAT(payload, StatusIs(PermanentError().code()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
