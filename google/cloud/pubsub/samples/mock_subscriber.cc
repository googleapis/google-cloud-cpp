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

//! [all]
//! [required-includes]
#include "google/cloud/pubsub/mocks/mock_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_subscriber_connection.h"
#include "google/cloud/pubsub/subscriber.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <future>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::google::cloud::promise;
using ::google::cloud::pubsub_mocks::MockAckHandler;
using ::google::cloud::pubsub_mocks::MockSubscriberConnection;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
namespace pubsub = ::google::cloud::pubsub;
//! [helper-aliases]

TEST(MockSubscribeExample, Subscribe) {
  //! [create-mock]
  auto mock = std::make_shared<MockSubscriberConnection>();
  //! [create-mock]

  // Generate 3 messages in a separate thread and then close the
  // subscription with success.
  //! [message-generator]
  auto generator =
      [](promise<google::cloud::Status> promise,
         pubsub::SubscriberConnection::SubscribeParams const& params) {
        for (int i = 0; i != 3; ++i) {
          //! [setup-mock-handler]
          auto mock_handler = absl::make_unique<MockAckHandler>();
          EXPECT_CALL(*mock_handler, ack_id)
              .WillRepeatedly(Return("ack-id-" + std::to_string(i)));
          EXPECT_CALL(*mock_handler, ack).Times(1);
          //! [setup-mock-handler]

          //! [simulate-callback]
          params.callback(pubsub::MessageBuilder{}
                              .SetData("message-" + std::to_string(i))
                              .Build(),
                          pubsub::AckHandler(std::move(mock_handler)));
          //! [simulate-callback]
        }
        // Close the stream with a successful error code
        promise.set_value({});
      };
  //! [message-generator]

  //! [setup-expectations]
  EXPECT_CALL(*mock, Subscribe)
      .WillOnce([&](pubsub::SubscriberConnection::SubscribeParams params) {
        promise<google::cloud::Status> p;
        auto result = p.get_future();
        // start the generator in a separate thread.
        (void)std::async(std::launch::async, generator, std::move(p),
                         std::move(params));
        return result;
      });
  //! [setup-expectations]

  //! [create-client]
  pubsub::Subscriber subscriber(mock);
  //! [create-client]

  //! [client-call]
  std::vector<std::string> payloads;
  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    payloads.push_back(m.data());
    std::move(h).ack();
  };
  auto session = subscriber.Subscribe(
      pubsub::Subscription("mock-project", "mock-subscription"), handler);
  //! [client-call]

  //! [expected-results]
  EXPECT_TRUE(session.get().ok());
  EXPECT_THAT(payloads,
              UnorderedElementsAre("message-0", "message-1", "message-2"));
  //! [expected-results]
}

}  // namespace

//! [all]
