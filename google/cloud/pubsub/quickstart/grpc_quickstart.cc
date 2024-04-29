// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [START pubsub_quickstart_subscriber] [all]
#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/opentelemetry_options.h"
#include <iostream>

// GOOGLE_CLOUD_CPP_ENABLE_TRACING=rpc,rpc-streams
// GOOGLE_CLOUD_CPP_ENABLE_CLOG=1 bazel run
// //google/cloud/pubsub/quickstart:grpc_quickstart gcloud pubsub topics create
// expire-topic gcloud pubsub subscriptions create expire-sub
// --topic=expire-topic
int main(int argc, char* argv[]) try {
  std::string const project_id = "alevenb-test";
  std::string const subscription_id = "expire-sub";

  auto constexpr kWaitTimeout = std::chrono::seconds(60);

  // Create a namespace alias to make the code easier to read.
  namespace pubsub = ::google::cloud::pubsub;
  namespace otel = ::google::cloud::otel;
  namespace experimental = ::google::cloud::experimental;
  namespace gc = ::google::cloud;

  auto project = gc::Project(project_id);
  auto configuration = otel::ConfigureBasicTracing(project);

  // Create a client with OpenTelemetry tracing enabled.
  auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
      pubsub::Subscription(project_id, subscription_id),
      gc::Options{}
          .set<gc::OpenTelemetryTracingOption>(true)
          .set<pubsub::MinDeadlineExtensionOption>(std::chrono::seconds(10))
          .set<pubsub::MaxDeadlineExtensionOption>(std::chrono::seconds(60))));
  std::string const topic_id = "expire-topic";
  auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
      pubsub::Topic(project_id, topic_id),
      gc::Options{}.set<gc::OpenTelemetryTracingOption>(true)));

  int n = 1;
  std::vector<gc::future<void>> ids;
  for (int i = 0; i < n; i++) {
    auto id =
        publisher
            .Publish(
                pubsub::MessageBuilder().SetData(std::to_string(i)).Build())
            .then([index = i](gc::future<gc::StatusOr<std::string>> f) {
              auto status = f.get();
              if (!status) {
                std::cout << "Error in publish: " << status.status() << "\n";
                return;
              }
              std::cout << index << ". ";
              std::cout << "Sent message with id: (" << *status << ")\n";
            });
    ids.push_back(std::move(id));
  }
  // Block until they are actually sent.
  for (auto& id : ids) id.get();

  std::unordered_set<std::string> messages;
  auto session =
      subscriber.Subscribe([&](pubsub::Message const& m, pubsub::AckHandler h) {
        std::cout << m.data() << ". ";
        std::cout << "Received message with id: (" << m.message_id() << ")\n";
        sleep(41);
      });

  std::cout << "Waiting for messages on " + subscription_id + "...\n";
  // Blocks until the timeout is reached.
  auto result = session.wait_for(kWaitTimeout);
  if (result == std::future_status::timeout) {
    std::cout << "timeout reached, ending session\n";
    session.cancel();
  }
  session =
      subscriber.Subscribe([&](pubsub::Message const& m, pubsub::AckHandler h) {
        std::cout << "Received message " << m << "\n";
        std::move(h).ack();
      });

  // Blocks until the timeout is reached.
  result = session.wait_for(std::chrono::seconds(10));

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [END pubsub_quickstart_subscriber] [all]