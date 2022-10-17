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

#include "google/cloud/oslogin/os_login_client.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " user\n";
    return 1;
  }

  namespace oslogin = ::google::cloud::oslogin;
  auto client =
      oslogin::OsLoginServiceClient(oslogin::MakeOsLoginServiceConnection());

  auto const name = "users/" + std::string{argv[1]};
  auto lp = client.GetLoginProfile(name);
  if (!lp) throw std::move(lp).status();
  std::cout << lp->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
