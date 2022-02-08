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

#include "google/cloud/contactcenterinsights/contact_center_insights_client.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace ccai = ::google::cloud::contactcenterinsights;
  auto client = ccai::ContactCenterInsightsClient(
      ccai::MakeContactCenterInsightsConnection());

  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + argv[2];
  for (auto c : client.ListConversations(parent)) {
    if (!c) throw std::runtime_error(c.status().message());

    using ::google::protobuf::util::TimeUtil;
    std::cout << c->name() << "\n";
    std::cout << "Duration: " << TimeUtil::ToString(c->duration())
              << "; Turns: " << c->turn_count() << "\n\n";
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
