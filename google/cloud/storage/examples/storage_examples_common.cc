// Copyright 2020 Google LLC
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

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace storage {
namespace examples {

Example::Example(std::map<std::string, CommandType> commands)
    : commands_(std::move(commands)) {
  // Force each command to generate its Usage string, so we can provide a good
  // usage string for the whole program.
  for (auto const& kv : commands_) {
    if (kv.first == "auto") continue;
    try {
      kv.second({});
    } catch (Usage const& u) {
      full_usage_ += "    ";
      full_usage_ += u.what();
      full_usage_ += "\n";
    }
  }
}

int Example::Run(int argc, char const* const argv[]) try {
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (argc == 1 && auto_run) {
    auto entry = commands_.find("auto");
    if (entry == commands_.end()) {
      PrintUsage(argv[0], "Requested auto run but there is no 'auto' command");
      return 1;
    }
    entry->second({});
    return 0;
  }

  if (argc < 2) {
    PrintUsage(argv[0], "Missing command");
    return 1;
  }

  std::string const command_name = argv[1];
  auto command = commands_.find(command_name);
  if (commands_.end() == command) {
    PrintUsage(argv[0], "Unknown command: " + command_name);
    return 1;
  }

  command->second({argv + 2, argv + argc});

  return 0;
} catch (Usage const& u) {
  PrintUsage(argv[0], u.what());
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}

void Example::PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = cmd.find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << full_usage_ << "\n";
}

}  // namespace examples
}  // namespace storage
}  // namespace cloud
}  // namespace google
