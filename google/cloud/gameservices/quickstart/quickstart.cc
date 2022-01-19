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

#include "google/cloud/gameservices/game_server_clusters_client.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id realm-id\n";
    return 1;
  }

  namespace gameservices = ::google::cloud::gameservices;
  auto client = gameservices::GameServerClustersServiceClient(
      gameservices::MakeGameServerClustersServiceConnection());

  auto const realm = "projects/" + std::string(argv[1]) + "/locations/" +
                     std::string(argv[2]) + "/realms/" + std::string(argv[3]);
  for (auto r : client.ListGameServerClusters(realm)) {
    if (!r) throw std::runtime_error(r.status().message());
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
