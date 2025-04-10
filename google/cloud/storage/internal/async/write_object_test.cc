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

#include "google/cloud/storage/internal/async/write_object.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::NotNull;

using MockStream = google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiWriteObjectRequest,
    google::storage::v2::BidiWriteObjectResponse>;

TEST(WriteObjectTest, Basic) {
  auto constexpr kText = R"pb(
    resource { bucket: "projects/_/buckets/test-bucket", name: "test-object" }
    appendable: true
  )pb";
  auto constexpr kWriteResponse = R"pb(
    persisted_size: 2048
    write_handle { handle: "handle-123" }
  )pb";

  auto request = google::storage::v2::BidiWriteObjectRequest{};
  ASSERT_TRUE(
      TextFormat::ParseFromString(kText, request.mutable_write_object_spec()));
  auto expected_response = google::storage::v2::BidiWriteObjectResponse{};
  ASSERT_TRUE(TextFormat::ParseFromString(kWriteResponse, &expected_response));

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Start).WillOnce([&sequencer]() {
    return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
  });

  EXPECT_CALL(*mock, Write)
      .WillOnce([&sequencer, request](
                    google::storage::v2::BidiWriteObjectRequest const& actual,
                    grpc::WriteOptions) {
        EXPECT_THAT(actual, IsProtoEqual(request));
        return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
      });

  EXPECT_CALL(*mock, Read).WillOnce([&sequencer, expected_response]() {
    return sequencer.PushBack("Read").then([expected_response](auto) {
      return absl::make_optional(expected_response);
    });
  });

  auto write_object = std::make_shared<WriteObject>(std::move(mock), request);
  auto pending = write_object->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto result = pending.get();
  auto expected_result = [expected_response]() {
    return AllOf(Field(&WriteObject::WriteResult::stream, NotNull()),
                 Field(&WriteObject::WriteResult::first_response,
                       IsProtoEqual(expected_response)));
  };
  ASSERT_THAT(result, IsOkAndHolds(expected_result()));
}

TEST(WriteObject, BasicWriteHandle) {
  auto constexpr kText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    generation: 42
    routing_token: "test-routing-token"
    write_handle { handle: "handle-123" }
  )pb";

  auto constexpr kWriteResponse = R"pb(
    resource { bucket: "projects/_/buckets/test-bucket" name: "test-object" }
    write_handle { handle: "handle-123" }
  )pb";

  auto request = google::storage::v2::BidiWriteObjectRequest{};
  ASSERT_TRUE(
      TextFormat::ParseFromString(kText, request.mutable_append_object_spec()));
  auto expected_response = google::storage::v2::BidiWriteObjectResponse{};
  ASSERT_TRUE(TextFormat::ParseFromString(kWriteResponse, &expected_response));

  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillOnce([&sequencer]() {
    return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Write)
      .WillOnce([&sequencer, expected_request = request](
                    google::storage::v2::BidiWriteObjectRequest const& request,
                    grpc::WriteOptions) {
        EXPECT_THAT(request, IsProtoEqual(expected_request));
        return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
      });
  EXPECT_CALL(*mock, Read).WillOnce([&sequencer, expected_response]() {
    return sequencer.PushBack("Read").then([expected_response](auto) {
      return absl::make_optional(expected_response);
    });
  });

  auto write_object = std::make_shared<WriteObject>(std::move(mock), request);
  auto pending = write_object->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(true);

  auto response = pending.get();
  auto expected_result = [expected_response]() {
    return AllOf(Field(&WriteObject::WriteResult::stream, NotNull()),
                 Field(&WriteObject::WriteResult::first_response,
                       IsProtoEqual(expected_response)));
  };
  ASSERT_THAT(response, IsOkAndHolds(expected_result()));
}

TEST(WriteObject, StartError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillOnce([&sequencer]() {
    return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });

  auto write_object = std::make_shared<WriteObject>(
      std::move(mock), google::storage::v2::BidiWriteObjectRequest{});
  auto pending = write_object->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto result = pending.get();
  EXPECT_THAT(result, StatusIs(PermanentError().code()));
}

TEST(WriteObject, WriteError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillOnce([&sequencer]() {
    return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Write).WillOnce([&sequencer]() {
    return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });

  auto write_object = std::make_shared<WriteObject>(
      std::move(mock), google::storage::v2::BidiWriteObjectRequest{});
  auto pending = write_object->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);  // simulate the error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto result = pending.get();
  EXPECT_THAT(result, StatusIs(PermanentError().code()));
}

TEST(WriteObject, ReadError) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillOnce([&sequencer]() {
    return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Write).WillOnce([&sequencer]() {
    return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Read).WillOnce([&sequencer]() {
    return sequencer.PushBack("Read").then([](auto) {
      return absl::optional<google::storage::v2::BidiWriteObjectResponse>();
    });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then(
        [](auto) { return PermanentError(); });
  });

  auto write_object = std::make_shared<WriteObject>(
      std::move(mock), google::storage::v2::BidiWriteObjectRequest{});
  auto pending = write_object->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(false);  // simulate the error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto result = pending.get();
  ASSERT_THAT(result, StatusIs(PermanentError().code()));
}

TEST(WriteObject, UnexpectedFinish) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();

  EXPECT_CALL(*mock, Start).WillOnce([&sequencer]() {
    return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Write).WillOnce([&sequencer]() {
    return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
  });
  EXPECT_CALL(*mock, Finish).WillOnce([&sequencer]() {
    return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
  });

  auto write_object = std::make_shared<WriteObject>(
      std::move(mock), google::storage::v2::BidiWriteObjectRequest{});
  auto pending = write_object->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);  // simulate the error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto result = pending.get();
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
