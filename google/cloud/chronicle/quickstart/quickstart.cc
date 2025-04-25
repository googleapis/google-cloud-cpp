// Copyright 2025 Google LLC
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

//! [all]
#include "google/cloud/chronicle/v1/entity_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id location-id instance-id\n";
    return 1;
  }
  auto const endpoint = "us-chronicle.googleapis.com";
  auto const location = google::cloud::Location(argv[1], argv[2]);
  auto const instance_id = std::string(argv[3]);

  namespace gc = ::google::cloud;
  namespace chronicle = ::google::cloud::chronicle_v1;

  auto client =
      chronicle::EntityServiceClient(chronicle::MakeEntityServiceConnection(
          gc::Options{}
              .set<gc::EndpointOption>(endpoint)
              .set<gc::AuthorityOption>(endpoint)));

  for (auto r : client.ListWatchlists(location.FullName() + "/instances/" +
                                      instance_id)) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
