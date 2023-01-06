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

//! [START iam_quickstart]
#include "google/cloud/iam/iam_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <project-id>\n";
    return 1;
  }

  // Create a namespace alias to make the code easier to read.
  namespace iam = ::google::cloud::iam;
  iam::IAMClient client(iam::MakeIAMConnection());
  auto const project = google::cloud::Project(argv[1]);
  std::cout << "Service Accounts for project: " << project.project_id() << "\n";
  int count = 0;
  for (auto sa : client.ListServiceAccounts(project.FullName())) {
    if (!sa) throw std::move(sa).status();
    std::cout << sa->name() << "\n";
    ++count;
  }
  if (count == 0) std::cout << "No Service Accounts found.\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [END iam_quickstart]
