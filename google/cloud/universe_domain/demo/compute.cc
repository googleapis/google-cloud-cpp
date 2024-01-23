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

#include "google/cloud/compute/disks/v1/disks_client.h"
#include "google/cloud/compute/disks/v1/disks_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_options.h"
#include "google/cloud/universe_domain.h"
#include "google/cloud/universe_domain_options.h"
#include <iostream>

namespace {

// This is necessary to access alternate compute API.
void AddTargetApiVersionFromEnvVar(google::cloud::Options& options) {
  auto compute_api = google::cloud::internal::GetEnv("COMPUTE_TARGET_API");
  if (compute_api) {
    options.set<google::cloud::rest_internal::TargetApiVersionOption>(
        *compute_api);
  }
}

}  // namespace

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const zone_id = argv[2];

  // Create aliases to make the code easier to read.
  namespace disks = ::google::cloud::compute_disks_v1;
  auto options =
      google::cloud::AddUniverseDomainOption(google::cloud::ExperimentalTag{});
  if (!options.ok()) throw std::move(options).status();

  // Override retry policy to quickly exit if there's a failure.
  options->set<disks::DisksRetryPolicyOption>(
      std::make_shared<disks::DisksLimitedErrorCountRetryPolicy>(3));
  // set env var COMPUTE_TARGET_API to select api other than "v1".
  AddTargetApiVersionFromEnvVar(*options);
  auto client = disks::DisksClient(disks::MakeDisksConnectionRest(*options));

  std::cout << "compute.ListDisks:\n";
  for (auto disk : client.ListDisks(project_id, zone_id)) {
    if (!disk) throw std::move(disk).status();
    std::cout << disk->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
