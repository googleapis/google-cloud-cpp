// Copyright 2020 Google LLC
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
#include "google/cloud/spanner/client.h"
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id instance-id database-id\n";
    return 1;
  }

  namespace spanner = ::google::cloud::spanner;
  spanner::Client client(
      spanner::MakeConnection(spanner::Database(argv[1], argv[2], argv[3])));

  auto rows =
      client.ExecuteQuery(spanner::SqlStatement("SELECT 'Hello World'"));

  for (auto const& row : spanner::StreamOf<std::tuple<std::string>>(rows)) {
    if (!row) {
      std::cerr << row.status() << "\n";
      return 1;
    }
    std::cout << std::get<0>(*row) << "\n";
  }

  return 0;
}
//! [all]
