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

#include "google/cloud/bigtable/emulator/server.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include <absl/strings/str_cat.h>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

ABSL_FLAG(std::string, host, "localhost",
          "the address to bind to on the local machine");
ABSL_FLAG(std::uint16_t, port, 8888,
          "the port to bind to on the local machine");

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      absl::StrCat("Usage: %s -h <host> -p <port>", argv[0]));
  absl::ParseCommandLine(argc, argv);

  auto maybe_server =
      google::cloud::bigtable::emulator::CreateDefaultEmulatorServer(
          absl::GetFlag(FLAGS_host), absl::GetFlag(FLAGS_port));
  if (!maybe_server) {
    std::cerr << "CreateDefaultEmulatorServer() failed. See logs for "
                 "possible reason"
              << std::endl;
    return 1;
  }

  auto& server = maybe_server.value();

  std::cout << "Server running on port " << server->bound_port() << "\n";
  server->Wait();
  return 0;
}
