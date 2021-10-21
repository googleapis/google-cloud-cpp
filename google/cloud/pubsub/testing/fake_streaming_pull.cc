// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/testing/fake_streaming_pull.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

using ::testing::AtLeast;
using ::testing::AtMost;

std::unique_ptr<pubsub_testing::MockAsyncPullStream> FakeAsyncStreamingPull(
    google::cloud::CompletionQueue& cq, std::unique_ptr<grpc::ClientContext>,
    google::pubsub::v1::StreamingPullRequest const&) {
  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
  using us = std::chrono::microseconds;

  auto start_response = [cq]() mutable {
    return cq.MakeRelativeTimer(us(10)).then([](TimerFuture) { return true; });
  };
  auto write_response = [cq](google::pubsub::v1::StreamingPullRequest const&,
                             grpc::WriteOptions const&) mutable {
    return cq.MakeRelativeTimer(us(10)).then([](TimerFuture) { return true; });
  };

  using Response = google::pubsub::v1::StreamingPullResponse;
  class MessageGenerator {
   public:
    MessageGenerator() = default;

    Response Generate(int n) {
      Response response;
      std::unique_lock<std::mutex> lk(mu_);
      for (int i = 0; i != n; ++i) {
        auto& m = *response.add_received_messages();
        m.set_ack_id("test-ack-id-" + std::to_string(count_));
        m.mutable_message()->set_message_id("test-message-id-" +
                                            std::to_string(count_));
        ++count_;
      }
      return response;
    }

   private:
    std::mutex mu_;
    int count_ = 0;
  };

  auto generator = std::make_shared<MessageGenerator>();
  auto read_response = [cq, generator]() mutable {
    return cq.MakeRelativeTimer(us(10)).then([generator](TimerFuture) {
      return absl::make_optional(generator->Generate(10));
    });
  };
  auto canceled_response = [cq]() mutable {
    return cq.MakeRelativeTimer(us(10)).then(
        [](TimerFuture) { return absl::optional<Response>{}; });
  };
  auto finish_response = [cq]() mutable {
    return cq.MakeRelativeTimer(us(10)).then(
        [](TimerFuture) { return Status{}; });
  };

  auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
  EXPECT_CALL(*stream, Start).WillOnce(start_response);
  EXPECT_CALL(*stream, Write).Times(AtLeast(1)).WillRepeatedly(write_response);
  EXPECT_CALL(*stream, Read).Times(AtLeast(1)).WillRepeatedly(read_response);
  ::testing::InSequence sequence;
  EXPECT_CALL(*stream, Cancel).Times(1);
  EXPECT_CALL(*stream, Read).Times(AtMost(1)).WillRepeatedly(canceled_response);
  EXPECT_CALL(*stream, Finish).Times(AtMost(1)).WillRepeatedly(finish_response);

  return stream;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google
