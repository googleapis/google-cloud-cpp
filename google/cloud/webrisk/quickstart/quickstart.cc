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

#include "google/cloud/webrisk/web_risk_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0]
              << " [uri (default https://www.google.com)]\n";
    return 1;
  }

  namespace webrisk = ::google::cloud::webrisk;
  auto client =
      webrisk::WebRiskServiceClient(webrisk::MakeWebRiskServiceConnection());

  auto const uri = std::string{argc == 2 ? argv[1] : "https://www.google.com/"};
  auto const threat_types = std::vector<webrisk::v1::ThreatType>{
      webrisk::v1::MALWARE, webrisk::v1::UNWANTED_SOFTWARE};
  auto response = client.SearchUris("https://www.google.com/", threat_types);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
