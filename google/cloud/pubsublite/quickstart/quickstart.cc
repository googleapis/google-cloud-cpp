// Copyright 2021 Google LLC
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

#include "google/cloud/pubsublite/admin_client.h"
#include "google/cloud/pubsublite/endpoint.h"
#include "google/cloud/common_options.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }

  namespace gc = ::google::cloud;
  namespace pubsublite = ::google::cloud::pubsublite;
  auto const zone_id = std::string{argv[2]};
  auto endpoint = pubsublite::EndpointFromZone(zone_id);
  if (!endpoint) throw std::runtime_error(endpoint.status().message());
  auto client =
      pubsublite::AdminServiceClient(pubsublite::MakeAdminServiceConnection(
          gc::Options{}
              .set<gc::EndpointOption>(*endpoint)
              .set<gc::AuthorityOption>(*endpoint)));
  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + zone_id;
  for (auto const& topic : client.ListTopics(parent)) {
    std::cout << topic.value().DebugString() << "\n";
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
