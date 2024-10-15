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

//! [all]
#include "google/cloud/iam/admin/v1/iam_client.h"
#include "google/cloud/iam/admin/v1/iam_options.h"
#include "google/cloud/location.h"
#include "google/cloud/universe_domain.h"
#include "google/cloud/universe_domain_options.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id location-id [sa-key-file]\n";
    return 1;
  }
  namespace gc = ::google::cloud;
  namespace iam = ::google::cloud::iam_admin_v1;
  auto const location = gc::Location(argv[1], argv[2]);
  auto const project = google::cloud::Project(argv[1]);

  gc::Options options;
  if (argc == 4) {
    auto is = std::ifstream(argv[3]);
    is.exceptions(std::ios::badbit);
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    options.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeServiceAccountCredentials(contents));
  }

  // Interrogate credentials for universe_domain and add the value to returned
  // options.
  auto ud_options = gc::AddUniverseDomainOption(gc::ExperimentalTag{}, options);
  if (!ud_options.ok()) throw std::move(ud_options).status();

  // Override retry policy to quickly exit if there's a failure.
//   ud_options->set<kms::KeyManagementServiceRetryPolicyOption>(
//       std::make_shared<kms::KeyManagementServiceLimitedErrorCountRetryPolicy>(
//           3));

  auto client = iam::IAMClient(
      iam::MakeIAMConnection(*ud_options));

//   std::cout << "kms.ListKeyRings:\n";
//   for (auto kr : client.ListKeyRings(location.FullName())) {
//     if (!kr) throw std::move(kr).status();
//     std::string name = kr->name();
//     std::cout << "short_key_name: " << name.substr(name.rfind('/') + 1) << "\n";
//     std::cout << kr->create_time().DebugString() << "\n";
//   }

  std::cout << "iam.ListServiceAccounts: " << project.FullName() << "\n";
  for (auto sa : client.ListServiceAccounts(project.FullName())) {
    if (!sa) throw std::move(sa).status();
    std::cout << sa->name() << "\n";
  }
  

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
