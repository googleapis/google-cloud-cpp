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
#include "google/cloud/pubsub/mocks/mock_publisher_connection.h"
#include "google/cloud/pubsub/publisher.h"
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::google::cloud::pubsub_mocks::MockPublisherConnection;
namespace pubsub = ::google::cloud::pubsub;
//! [helper-aliases]

TEST(MockPublishExample, PublishSimple) {
  //! [create-mock]
  auto mock = std::make_shared<MockPublisherConnection>();
  //! [create-mock]

  //! [setup-expectations]
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](pubsub::PublisherConnection::PublishParams const& p) {
        EXPECT_EQ("test-data-0", p.message.data());
        return google::cloud::make_ready_future(
            google::cloud::StatusOr<std::string>("test-id-0"));
      });
  //! [setup-expectations]

  //! [create-client]
  pubsub::Publisher publisher(mock);
  //! [create-client]

  //! [client-call]
  auto id =
      publisher.Publish(pubsub::MessageBuilder{}.SetData("test-data-0").Build())
          .get();
  //! [client-call]

  //! [expected-results]
  EXPECT_TRUE(id.ok());
  EXPECT_EQ("test-id-0", *id);
  //! [expected-results]
}

}  // namespace

//! [all]
