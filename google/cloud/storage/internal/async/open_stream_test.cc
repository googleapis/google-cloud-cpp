// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/open_stream.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/storage/v2/storage.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::StatusIs;
using ReadType = ::google::cloud::storage_internal::OpenStream::ReadType;

using MockStream = google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiReadObjectRequest,
    google::storage::v2::BidiReadObjectResponse>;

TEST(OpenStream, CancelBlocksAllRequest) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStream>();
  EXPECT_CALL(*mock, Write).Times(0);
  EXPECT_CALL(*mock, Read).Times(0);
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer] {
    return sequencer.PushBack("Finish").then([](auto) {
      return internal::CancelledError("test-only", GCP_ERROR_INFO());
    });
  });

  auto actual = std::make_shared<OpenStream>(std::move(mock));
  actual->Cancel();

  EXPECT_FALSE(actual->Write({}).get());
  EXPECT_FALSE(actual->Read().get().has_value());

  actual.reset();

  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

TEST(OpenStream, DuplicateFinish) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer] {
    return sequencer.PushBack("Finish").then([](auto) {
      return internal::CancelledError("test-only", GCP_ERROR_INFO());
    });
  });

  auto actual = std::make_shared<OpenStream>(std::move(mock));

  auto f1 = actual->Finish();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  EXPECT_THAT(f1.get(), StatusIs(StatusCode::kCancelled));

  actual->Cancel();
  actual.reset();
}

TEST(OpenStream, CleanShutdown) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_shared<MockStream>();
  EXPECT_CALL(*mock, Write).WillOnce([&sequencer] {
    return sequencer.PushBack("Write").then([](auto) { return false; });
  });
  EXPECT_CALL(*mock, Read).WillOnce([&sequencer] {
    return sequencer.PushBack("Read").then(
        [](auto) { return ReadType(absl::nullopt); });
  });
  EXPECT_CALL(*mock, Cancel).Times(1);
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer] {
    return sequencer.PushBack("Finish").then([](auto) {
      return internal::CancelledError("test-only", GCP_ERROR_INFO());
    });
  });

  auto actual = std::make_shared<OpenStream>(std::move(mock));
  auto write = actual->Write(google::storage::v2::BidiReadObjectRequest{});
  auto ws = sequencer.PopFrontWithName();
  EXPECT_EQ(ws.second, "Write");
  auto read = actual->Read();
  auto rs = sequencer.PopFrontWithName();
  EXPECT_EQ(rs.second, "Read");

  actual->Cancel();
  actual.reset();

  ws.first.set_value(true);
  rs.first.set_value(true);
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
