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

//! [all] [START storage_control_quickstart_sample]
#include "google/cloud/storagecontrol/v2/storage_control_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " bucket-id\n";
    return 1;
  }
  auto const name =
      std::string{"projects/_/buckets/"} + argv[1] + "/storageLayout";

  namespace storagecontrol = ::google::cloud::storagecontrol_v2;
  auto client = storagecontrol::StorageControlClient(
      storagecontrol::MakeStorageControlConnection());

  auto layout = client.GetStorageLayout(name);
  if (!layout) throw std::move(layout).status();
  std::cout << layout->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all] [END storage_control_quickstart_sample]
