// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [START pubsub_quickstart_publisher]
#include "google/cloud/pubsub/publisher.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <project-id> <topic-id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const topic_id = argv[2];

  // Create a namespace alias to make the code easier to read.
  namespace pubsub = ::google::cloud::pubsub;
  auto publisher = pubsub::Publisher(
      pubsub::MakePublisherConnection(pubsub::Topic(project_id, topic_id), {}));
  auto id =
      publisher
          .Publish(pubsub::MessageBuilder{}.SetData("Hello World!").Build())
          .get();
  if (!id) throw std::runtime_error(id.status().message());
  std::cout << "Hello World published with id=" << *id << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
//! [END pubsub_quickstart_publisher]
