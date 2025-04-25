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

#include "google/cloud/storage/internal/async/open_object.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pair;

using MockStream = google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
    google::storage::v2::BidiReadObjectRequest,
    google::storage::v2::BidiReadObjectResponse>;

TEST(OpenImpl, RequestParams) {
  auto constexpr kPlain = R"pb(
    read_object_spec {
      bucket: "projects/_/buckets/test-bucket-name"
      object: "test-object-unused"
      generation: 42
      read_handle { handle: "unused" }
    }
  )pb";
  auto constexpr kWithRoutingToken = R"pb(
    read_object_spec {
      bucket: "projects/_/buckets/test-bucket-name"
      object: "test-object-unused"
      generation: 42
      read_handle { handle: "unused" }
      routing_token: "test-routing-token"
    }
  )pb";

  auto params = [](auto text) {
    auto r = google::storage::v2::BidiReadObjectRequest{};
    TextFormat::ParseFromString(text, &r);
    return RequestParams(r);
  };
  EXPECT_EQ(params(kPlain), "bucket=projects/_/buckets/test-bucket-name");
  EXPECT_EQ(params(kWithRoutingToken),
            "bucket=projects/_/buckets/test-bucket-name"
            "&routing_token=test-routing-token");
}

TEST(OpenImpl, Basic) {
  auto constexpr kText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    generation: 42
  )pb";
  auto constexpr kReadResponse = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-123" }
  )pb";

  auto request = google::storage::v2::BidiReadObjectRequest{};
  ASSERT_TRUE(
      TextFormat::ParseFromString(kText, request.mutable_read_object_spec()));
  auto expected_response = google::storage::v2::BidiReadObjectResponse{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadResponse, &expected_response));

  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject)
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const& context,
                    google::cloud::internal::ImmutableOptions const&) {
        google::cloud::testing_util::ValidateMetadataFixture md;
        auto metadata = md.GetMetadata(*context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-request-params",
                                  "bucket=projects/_/buckets/test-bucket")));
        auto stream = std::make_unique<MockStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
          return sequencer.PushBack("Start").then(
              [](auto f) { return f.get(); });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&sequencer, request](
                    google::storage::v2::BidiReadObjectRequest const& actual,
                    grpc::WriteOptions) {
                  EXPECT_THAT(actual, IsProtoEqual(request));
                  return sequencer.PushBack("Write").then(
                      [](auto f) { return f.get(); });
                });
        EXPECT_CALL(*stream, Read).WillOnce([&sequencer, expected_response]() {
          return sequencer.PushBack("Read").then([expected_response](auto) {
            return absl::make_optional(expected_response);
          });
        });
        return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
      });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  auto pending = coro->Call();
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
    return AllOf(Field(&OpenStreamResult::stream, NotNull()),
                 Field(&OpenStreamResult::first_response,
                       IsProtoEqual(expected_response)));
  };
  ASSERT_THAT(response, IsOkAndHolds(expected_result()));
}

TEST(OpenImpl, BasicReadHandle) {
  auto constexpr kText = R"pb(
    bucket: "projects/_/buckets/test-bucket"
    object: "test-object"
    generation: 42
    read_handle { handle: "test-handle-1234" }
    routing_token: "test-token"
  )pb";
  auto constexpr kReadResponse = R"pb(
    metadata {
      bucket: "projects/_/buckets/test-bucket"
      name: "test-object"
      generation: 42
    }
    read_handle { handle: "handle-123" }
  )pb";

  auto request = google::storage::v2::BidiReadObjectRequest{};
  ASSERT_TRUE(
      TextFormat::ParseFromString(kText, request.mutable_read_object_spec()));
  auto expected_response = google::storage::v2::BidiReadObjectResponse{};
  ASSERT_TRUE(TextFormat::ParseFromString(kReadResponse, &expected_response));

  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject)
      .WillOnce([&](CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const& context,
                    google::cloud::internal::ImmutableOptions const&) {
        google::cloud::testing_util::ValidateMetadataFixture md;
        auto metadata = md.GetMetadata(*context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-request-params",
                                  "bucket=projects/_/buckets/test-bucket"
                                  "&routing_token=test-token")));

        auto stream = std::make_unique<MockStream>();
        EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
          return sequencer.PushBack("Start").then(
              [](auto f) { return f.get(); });
        });
        EXPECT_CALL(*stream, Write)
            .WillOnce(
                [&sequencer, expected_request = request](
                    google::storage::v2::BidiReadObjectRequest const& request,
                    grpc::WriteOptions) {
                  EXPECT_THAT(request, IsProtoEqual(expected_request));
                  return sequencer.PushBack("Write").then(
                      [](auto f) { return f.get(); });
                });
        EXPECT_CALL(*stream, Read).WillOnce([&sequencer, expected_response]() {
          return sequencer.PushBack("Read").then([expected_response](auto) {
            return absl::make_optional(expected_response);
          });
        });
        return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
      });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  auto pending = coro->Call();
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
    return AllOf(Field(&OpenStreamResult::stream, NotNull()),
                 Field(&OpenStreamResult::first_response,
                       IsProtoEqual(expected_response)));
  };
  ASSERT_THAT(response, IsOkAndHolds(expected_result()));
}

TEST(OpenImpl, StartError) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return PermanentError(); });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST(OpenImpl, WriteError) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return PermanentError(); });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST(OpenImpl, ReadError) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Read).WillOnce([&sequencer]() {
      return sequencer.PushBack("Read").then([](auto) {
        return absl::optional<google::storage::v2::BidiReadObjectResponse>();
      });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then(
          [](auto) { return PermanentError(); });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Read");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(PermanentError().code()));
}

TEST(OpenImpl, UnexpectedFinish) {
  AsyncSequencer<bool> sequencer;
  MockStorageStub mock;
  EXPECT_CALL(mock, AsyncBidiReadObject).WillOnce([&sequencer]() {
    auto stream = std::make_unique<MockStream>();
    EXPECT_CALL(*stream, Start).WillOnce([&sequencer]() {
      return sequencer.PushBack("Start").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Write).WillOnce([&sequencer]() {
      return sequencer.PushBack("Write").then([](auto f) { return f.get(); });
    });
    EXPECT_CALL(*stream, Finish).WillOnce([&sequencer]() {
      return sequencer.PushBack("Finish").then([](auto) { return Status{}; });
    });
    return std::unique_ptr<OpenStream::StreamingRpc>(std::move(stream));
  });

  CompletionQueue cq;
  auto coro = std::make_shared<OpenObject>(
      mock, cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}),
      google::storage::v2::BidiReadObjectRequest{});
  auto pending = coro->Call();
  auto next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Start");
  next.first.set_value(true);
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Write");
  next.first.set_value(false);  // simulate an error
  next = sequencer.PopFrontWithName();
  EXPECT_EQ(next.second, "Finish");
  next.first.set_value(true);

  auto response = pending.get();
  EXPECT_THAT(response, StatusIs(StatusCode::kInternal));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
