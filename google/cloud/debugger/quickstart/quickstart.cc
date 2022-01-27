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

#include "google/cloud/debugger/debugger2_client.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " debuggee-id client-version\n";
    return 1;
  }

  auto const debuggee_id = argv[1];
  auto const client_version = argv[2];
  namespace debugger = ::google::cloud::debugger;
  auto client = debugger::Debugger2Client(debugger::MakeDebugger2Connection());
  auto response = client.ListBreakpoints(debuggee_id, client_version);
  if (!response) throw std::runtime_error(response.status().message());
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
