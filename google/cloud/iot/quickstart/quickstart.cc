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

#include "google/cloud/iot/device_manager_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace iot = ::google::cloud::iot;
  auto client = iot::DeviceManagerClient(iot::MakeDeviceManagerConnection());

  auto const project = google::cloud::Project(argv[1]);
  auto const parent = project.FullName() + "/locations/" + argv[2];
  for (auto dr : client.ListDeviceRegistries(parent)) {
    if (!dr) throw std::move(dr).status();
    std::cout << dr->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
