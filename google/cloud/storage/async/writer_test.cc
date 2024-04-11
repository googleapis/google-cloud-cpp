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

#include "google/cloud/storage/async/writer.h"
#include "google/cloud/storage/mocks/mock_async_writer_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage_experimental::WritePayload;
using ::google::cloud::storage_mocks::MockAsyncWriterConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

template <typename Matcher>
auto WritePayloadContents(Matcher&& m) {
  return ResultOf(
      "WritePayload contents are",
      [](WritePayload const& p) { return p.payload(); },
      std::forward<Matcher>(m));
}

TEST(AsyncWriterTest, Basic) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadId).WillOnce(Return("test-upload-id"));
  EXPECT_CALL(*mock, PersistedState).WillOnce(Return(16384));
  EXPECT_CALL(*mock, Write(WritePayloadContents(ElementsAre("aaa"))))
      .WillOnce([] { return make_ready_future(Status{}); });
  EXPECT_CALL(*mock, Write(WritePayloadContents(ElementsAre("bbb"))))
      .WillOnce([] { return make_ready_future(Status{}); });
  EXPECT_CALL(*mock, Finalize(WritePayloadContents(ElementsAre("ccc"))))
      .WillOnce([] {
        return make_ready_future(make_status_or(google::storage::v2::Object{}));
      });
  EXPECT_CALL(*mock, GetRequestMetadata)
      .WillOnce(Return(RpcMetadata{{{"hk0", "v0"}, {"hk1", "v1"}},
                                   {{"tk0", "v0"}, {"tk1", "v1"}}}));

  auto token = storage_internal::MakeAsyncToken(mock.get());
  AsyncWriter writer(std::move(mock));
  EXPECT_EQ(writer.UploadId(), "test-upload-id");
  EXPECT_THAT(writer.PersistedState(), VariantWith<std::int64_t>(16384));
  token = writer.Write(std::move(token), WritePayload{std::string("aaa")})
              .get()
              .value();
  token = writer.Write(std::move(token), WritePayload{std::string("bbb")})
              .get()
              .value();
  auto const actual =
      writer.Finalize(std::move(token), WritePayload{std::string("ccc")}).get();
  EXPECT_STATUS_OK(actual);

  auto const metadata = writer.GetRequestMetadata();
  EXPECT_THAT(metadata.headers,
              UnorderedElementsAre(Pair("hk0", "v0"), Pair("hk1", "v1")));
  EXPECT_THAT(metadata.trailers,
              UnorderedElementsAre(Pair("tk0", "v0"), Pair("tk1", "v1")));
}

TEST(AsyncWriterTest, FinalizeEmpty) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, Write(WritePayloadContents(ElementsAre("aaa"))))
      .WillOnce([] { return make_ready_future(Status{}); });
  EXPECT_CALL(*mock, Write(WritePayloadContents(ElementsAre("bbb"))))
      .WillOnce([] { return make_ready_future(Status{}); });
  EXPECT_CALL(*mock, Finalize(WritePayloadContents(IsEmpty()))).WillOnce([] {
    return make_ready_future(make_status_or(google::storage::v2::Object{}));
  });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  AsyncWriter writer(std::move(mock));
  token = writer.Write(std::move(token), WritePayload{std::string("aaa")})
              .get()
              .value();
  token = writer.Write(std::move(token), WritePayload{std::string("bbb")})
              .get()
              .value();
  auto const actual = writer.Finalize(std::move(token)).get();
  EXPECT_STATUS_OK(actual);
}

TEST(AsyncWriterTest, ErrorDuringWrite) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Write).WillOnce([] {
    return make_ready_future(PermanentError());
  });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  AsyncWriter writer(std::move(mock));
  auto const actual = writer.Write(std::move(token), WritePayload{}).get();
  EXPECT_THAT(actual, StatusIs(PermanentError().code()));
}

TEST(AsyncWriterTest, ErrorOnFinalize) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();
  EXPECT_CALL(*mock, Finalize).WillOnce([] {
    return make_ready_future(
        StatusOr<google::storage::v2::Object>(PermanentError()));
  });

  auto token = storage_internal::MakeAsyncToken(mock.get());
  AsyncWriter writer(std::move(mock));
  auto const actual = writer.Finalize(std::move(token), WritePayload{}).get();
  EXPECT_THAT(actual, StatusIs(PermanentError().code()));
}

TEST(AsyncWriterTest, ErrorOnClosedStream) {
  AsyncWriter writer;
  int placeholder = 0;
  auto const actual =
      writer
          .Write(storage_internal::MakeAsyncToken(&placeholder), WritePayload{})
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
}

TEST(AsyncWriterTest, ErrorWithInvalidToken) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();

  AsyncWriter writer(std::move(mock));
  auto const actual =
      writer.Write(storage_experimental::AsyncToken(), WritePayload{}).get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST(AsyncWriterTest, ErrorWithMismatchedToken) {
  auto mock = std::make_unique<MockAsyncWriterConnection>();

  AsyncWriter writer(std::move(mock));
  int placeholder = 0;
  auto const actual =
      writer
          .Write(storage_internal::MakeAsyncToken(&placeholder), WritePayload{})
          .get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
