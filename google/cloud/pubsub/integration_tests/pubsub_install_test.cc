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

#include "google/cloud/pubsub/publisher_client.h"
#include "google/cloud/internal/getenv.h"
#include <google/pubsub/v1/pubsub.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << "\n";
    return 1;
  }

  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error(
        "The GOOGLE_CLOUD_PROJECT environment variable should be set to a "
        "non-empty value");
  }

  std::cout << "Cloud Pub/Sub C++ Client version: "
            << google::cloud::version_string() << "\n";

  auto publisher = google::cloud::pubsub::PublisherClient(
      google::cloud::pubsub::MakePublisherConnection());
  std::cout << "Available topics in project " << project_id << ":\n";
  for (auto const& topic : publisher.ListTopics(project_id)) {
    if (!topic) throw std::runtime_error(topic.status().message());
    std::cout << topic->name() << "\n";
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
