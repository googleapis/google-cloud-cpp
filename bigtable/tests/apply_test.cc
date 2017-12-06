// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bigtable/client/data.h"

int main(int argc, char* argv[]) try {
  // Make sure the arguments are valid.
  if (argc != 5) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of("/");
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <table> <family>" << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_name = argv[3];
  std::string const family = argv[4];

  bigtable::Client client(project_id, instance_id);
  std::unique_ptr<bigtable::Table> table = client.Open(table_name);

  // TODO(#29) we should read these rows back when we have a read path
  auto mutation = bigtable::SingleRowMutation("row-key-0");
  mutation.emplace_back(bigtable::SetCell(family, "col0", 0, "value-0-0"));
  mutation.emplace_back(bigtable::SetCell(family, "col1", 0, "value-0-1"));
  table->Apply(std::move(mutation));
  std::cout << "row-key-0 mutated successfully\n";

  mutation = bigtable::SingleRowMutation("row-key-1");
  mutation.emplace_back(bigtable::SetCell(family, "col0", 0, "value-1-0"));
  mutation.emplace_back(bigtable::SetCell(family, "col1", 0, "value-1-1"));
  table->Apply(std::move(mutation));
  std::cout << "row-key-1 mutated successfully\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}
