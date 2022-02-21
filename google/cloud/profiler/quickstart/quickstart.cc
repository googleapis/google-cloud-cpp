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

#include "google/cloud/cloudprofiler/profiler_client.h"
#include "google/cloud/project.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace cloudprofiler = ::google::cloud::cloudprofiler;
  auto client = cloudprofiler::ProfilerServiceClient(
      cloudprofiler::MakeProfilerServiceConnection());

  google::devtools::cloudprofiler::v2::CreateProfileRequest req;
  req.set_parent(google::cloud::Project(argv[1]).FullName());
  req.add_profile_type(google::devtools::cloudprofiler::v2::CPU);
  auto& deployment = *req.mutable_deployment();
  deployment.set_project_id(argv[1]);
  deployment.set_target("quickstart");

  auto profile = client.CreateProfile(req);
  if (!profile) throw std::runtime_error(profile.status().message());
  std::cout << profile->DebugString() << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
