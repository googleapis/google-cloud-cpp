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

#include "google/cloud/policytroubleshooter/iam_checker_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " principal resource-name"
              << " permission\n";
    return 1;
  }

  namespace policytroubleshooter = ::google::cloud::policytroubleshooter;
  auto client = policytroubleshooter::IamCheckerClient(
      policytroubleshooter::MakeIamCheckerConnection());

  policytroubleshooter::v1::TroubleshootIamPolicyRequest request;
  auto& access_tuple = *request.mutable_access_tuple();
  access_tuple.set_principal(argv[1]);
  access_tuple.set_full_resource_name(argv[2]);
  access_tuple.set_permission(argv[3]);
  auto const response = client.TroubleshootIamPolicy(request);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
