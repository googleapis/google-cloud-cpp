// Copyright 2024 Google LLC
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

#include "google/cloud/pubsub/admin/topic_admin_client.h"
#include "google/cloud/pubsub/admin/topic_admin_options.h"
#include "google/cloud/universe_domain.h"
#include "google/cloud/universe_domain_options.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <project-id>\n";
    return 1;
  }
  std::string const project_id = std::string{"projects/"} + argv[1];

  // Create a namespace alias to make the code easier to read.
  namespace pubsub_admin = ::google::cloud::pubsub_admin;

  auto options = google::cloud::AddUniverseDomainOption();
  if (!options.ok()) throw std::move(options).status();

  // Override retry policy to quickly exit if there's a failure.
  options->set<pubsub_admin::TopicAdminRetryPolicyOption>(
      std::make_shared<pubsub_admin::TopicAdminLimitedErrorCountRetryPolicy>(
          3));
  auto topic_admin_client = pubsub_admin::TopicAdminClient(
      pubsub_admin::MakeTopicAdminConnection(*options));

  std::cout << "pubsub.ListTopics:\n";
  for (auto t : topic_admin_client.ListTopics(project_id)) {
    if (!t) throw std::move(t).status();
    std::cout << t->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
