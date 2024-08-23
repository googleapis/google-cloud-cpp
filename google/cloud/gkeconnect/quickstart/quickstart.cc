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
#include "google/cloud/gkeconnect/gateway/v1/gateway_control_client.h"
#include "absl/strings/str_cat.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id location-id membership-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace gkeconnect = ::google::cloud::gkeconnect_gateway_v1;
  auto client = gkeconnect::GatewayControlClient(
      gkeconnect::MakeGatewayControlConnection());

  google::cloud::gkeconnect::gateway::v1::GenerateCredentialsRequest request;
  request.set_name(absl::StrCat(location.FullName(), "/memberships/", argv[3]));

  auto response = client.GenerateCredentials(request);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
