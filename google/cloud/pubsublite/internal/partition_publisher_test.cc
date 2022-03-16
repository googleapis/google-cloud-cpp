// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/internal/partition_publisher.h"
#include "google/cloud/pubsublite/testing/mock_async_reader_writer.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using ::google::cloud::testing_util::IsProtoEqual;

using google::cloud::pubsublite::v1::Cursor;

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using ::google::cloud::pubsublite_testing::MockAsyncReaderWriter;

using AsyncReaderWriter =
    MockAsyncReaderWriter<PublishRequest, PublishResponse>;

using AsyncReadWriteStreamReturnType = std::unique_ptr<
    AsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

class PartitionPublisherBatchingTest : public ::testing::Test {
 protected:
  PartitionPublisherBatchingTest() = default;

  using MessagePromisePair =
      std::pair<PubSubMessage, promise<StatusOr<Cursor>>>;

  static std::deque<std::deque<MessagePromisePair>> TestCreateBatches(
      std::deque<MessagePromisePair> messages, BatchingOptions const& options) {
    std::deque<PartitionPublisher::MessageWithPromise> message_with_futures;
    for (auto& message_with_future : messages) {
      message_with_futures.emplace_back(PartitionPublisher::MessageWithPromise{
          std::move(message_with_future.first),
          std::move(message_with_future.second)});
    }
    auto batches = PartitionPublisher::CreateBatches(
        std::move(message_with_futures), std::move(options));
    std::deque<std::deque<MessagePromisePair>> ret_batches;
    for (auto& batch : batches) {
      std::deque<MessagePromisePair> message_batch;
      for (auto& message_with_future : batch) {
        message_batch.emplace_back(
            MessagePromisePair{std::move(message_with_future.message),
                               std::move(message_with_future.message_promise)});
      }
      ret_batches.push_back(std::move(message_batch));
    }
    return ret_batches;
  }
};

// batching logic
TEST_F(PartitionPublisherBatchingTest, SingleMessageBatch) {
  std::deque<MessagePromisePair> message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 10; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable, std::to_string(i)));
        });
    message_with_futures.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_message_count(1);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), messages.size());
  for (unsigned int i = 0; i < messages.size(); ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 1);
    EXPECT_THAT(batch[0].first, IsProtoEqual(messages[i]));
    batch[0].second.set_value(
        StatusOr<Cursor>{Status(StatusCode::kUnavailable, std::to_string(i))});
  }
}

TEST_F(PartitionPublisherBatchingTest,
       SingleMessageBatchMessageSizeRestriction) {
  std::deque<MessagePromisePair> message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 10; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable, std::to_string(i)));
        });
    message_with_futures.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_bytes(1);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), messages.size());
  for (unsigned int i = 0; i < messages.size(); ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 1);
    EXPECT_THAT(batch[0].first, IsProtoEqual(messages[i]));
    batch[0].second.set_value(
        StatusOr<Cursor>{Status(StatusCode::kUnavailable, std::to_string(i))});
  }
}

TEST_F(PartitionPublisherBatchingTest, FullAndPartialBatches) {
  std::deque<MessagePromisePair> message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 10; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable,
                           absl::StrCat("batch:", std::to_string(i / 3),
                                        "offset:", std::to_string(i))));
        });
    message_with_futures.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_message_count(3);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), ceil(((double)messages.size()) / 3.0));
  for (unsigned int i = 0; i < batches.size(); ++i) {
    auto& batch = batches[i];
    if (i < batches.size() - 1) {
      EXPECT_EQ(batch.size(), 3);
    } else {
      EXPECT_EQ(batch.size(), messages.size() % 3);
    }
    for (unsigned int j = 0; j < batch.size(); ++j) {
      EXPECT_THAT(batch[j].first, IsProtoEqual(messages[i * 3 + j]));
      batch[j].second.set_value(StatusOr<Cursor>{
          Status(StatusCode::kUnavailable,
                 absl::StrCat("batch:", std::to_string(i),
                              "offset:", std::to_string(i * 3 + j)))});
    }
  }
}

TEST_F(PartitionPublisherBatchingTest, FullBatchesMessageSizeRestriction) {
  std::deque<MessagePromisePair> message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 9; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable,
                           absl::StrCat("batch:", std::to_string(i / 3),
                                        "offset:", std::to_string(i))));
        });
    message_with_futures.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_bytes(messages[0].ByteSizeLong() * 3);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), ceil(((double)messages.size()) / 3.0));
  for (unsigned int i = 0; i < batches.size(); ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 3);
    for (unsigned int j = 0; j < batch.size(); ++j) {
      EXPECT_THAT(batch[j].first, IsProtoEqual(messages[i * 3 + j]));
      batch[j].second.set_value(StatusOr<Cursor>{
          Status(StatusCode::kUnavailable,
                 absl::StrCat("batch:", std::to_string(i),
                              "offset:", std::to_string(i * 3 + j)))});
    }
  }
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
