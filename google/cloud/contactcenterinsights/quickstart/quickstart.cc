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

//! [all]
#include "google/cloud/contactcenterinsights/v1/contact_center_insights_client.h"
#include "google/cloud/location.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace ccai = ::google::cloud::contactcenterinsights_v1;
  auto client = ccai::ContactCenterInsightsClient(
      ccai::MakeContactCenterInsightsConnection());

  for (auto c : client.ListConversations(location.FullName())) {
    if (!c) throw std::move(c).status();

    using ::google::protobuf::util::TimeUtil;
    std::cout << c->name() << "\n";
    std::cout << "Duration: " << TimeUtil::ToString(c->duration())
              << "; Turns: " << c->turn_count() << "\n\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
