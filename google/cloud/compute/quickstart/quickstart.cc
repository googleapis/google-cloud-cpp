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

//! [all]
#include "google/cloud/compute/instances/v1/instances_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }

  namespace instances = ::google::cloud::compute_instances_v1;
  auto client = instances::InstancesClient(
      google::cloud::ExperimentalTag{},
      instances::MakeInstancesConnectionRest(google::cloud::ExperimentalTag{}));

  google::cloud::cpp::compute::instances::v1::ListInstancesRequest request;
  request.set_project(argv[1]);
  request.set_zone(argv[2]);
  request.set_max_results(1);
  for (auto instance : client.ListInstances(std::move(request))) {
    if (!instance) throw std::move(instance).status();
    std::cout << instance->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
