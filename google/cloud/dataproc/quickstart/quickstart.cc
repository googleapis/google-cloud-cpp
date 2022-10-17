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

#include "google/cloud/dataproc/cluster_controller_client.h"
#include "google/cloud/common_options.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region\n";
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const region = argv[2];

  namespace dataproc = ::google::cloud::dataproc;

  auto client = dataproc::ClusterControllerClient(
      dataproc::MakeClusterControllerConnection(region == "global" ? ""
                                                                   : region));

  for (auto c : client.ListClusters(project_id, region)) {
    if (!c) throw std::move(c).status();
    std::cout << c->cluster_name() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
