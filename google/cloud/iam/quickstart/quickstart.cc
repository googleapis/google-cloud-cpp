// Copyright 2021 Google LLC
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

//! [START iam_quickstart]
#include "google/cloud/iam/iam_client.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <project-id>\n";
    return 1;
  }

  std::string const project_id = argv[1];

  // Create a namespace alias to make the code easier to read.
  namespace iam = google::cloud::iam;
  iam::IAMClient client(iam::MakeIAMConnection());
  std::cout << "Service Accounts for project: " << project_id << "\n";
  int count = 0;
  for (auto const& service_account :
       client.ListServiceAccounts("projects/" + project_id)) {
    if (!service_account) {
      throw std::runtime_error(service_account.status().message());
    }
    std::cout << service_account->name() << "\n";
    ++count;
  }

  if (count == 0) std::cout << "No Service Accounts found.\n";
  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
//! [END iam_quickstart]
