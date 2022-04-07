// Copyright 2022 Google LLC
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

#include "google/cloud/dialogflow_cx/agents_client.h"
#include "google/cloud/common_options.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region-id\n";
    return 1;
  }

  auto const project = std::string{argv[1]};
  auto const region = std::string{argv[2]};
  namespace dialogflow_cx = ::google::cloud::dialogflow_cx;
  namespace gc = ::google::cloud;

  auto options =
      gc::Options{}
          .set<gc::EndpointOption>(region + "-dialogflow.googleapis.com")
          .set<gc::AuthorityOption>(region + "-dialogflow.googleapis.com");
  auto client = dialogflow_cx::AgentsClient(
      dialogflow_cx::MakeAgentsConnection(std::move(options)));

  auto const location = "projects/" + project + "/locations/" + region;
  for (auto a : client.ListAgents(location)) {
    if (!a) throw std::runtime_error(a.status().message());
    std::cout << a->DebugString() << "\n";
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
