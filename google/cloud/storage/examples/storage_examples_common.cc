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
#include <sstream>

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
      kv.second({"--help"});
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

bool UsingTestbench() {
  return !google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
              .value_or("")
              .empty();
}

void CheckEnvironmentVariablesAreSet(std::vector<std::string> const& vars) {
  for (auto const& var : vars) {
    auto const value = google::cloud::internal::GetEnv(var.c_str());
    if (!value) {
      throw std::runtime_error("The " + var +
                               " environment variable is not set");
    }
    if (value->empty()) {
      throw std::runtime_error("The " + var +
                               " environment variable has an empty value");
    }
  }
}

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of a bucket name must be <= 63 characters,
  static std::size_t const kMaxBucketNameLength = 63;
  std::size_t const max_random_characters =
      kMaxBucketNameLength - prefix.size();
  // bucket names might also contain `-` and `_` characters, but we do not
  // *need* to use them.
  return prefix + google::cloud::internal::Sample(
                      gen, static_cast<int>(max_random_characters),
                      "abcdefghijklmnopqrstuvwxyz012456789");
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of an object name is something like 1024 characters (UTF-8
  // encoded), but we do not need that many.
  static std::size_t const kMaxObjectNameLength = 128;
  std::size_t const max_random_characters =
      kMaxObjectNameLength - prefix.size();
  // object names might contain all kinds of characters, but we can use just
  // a subset for these purposes.
  return prefix + google::cloud::internal::Sample(
                      gen, static_cast<int>(max_random_characters),
                      "abcdefghijklmnopqrstuvwxyz012456789");
}

Commands::value_type CreateCommandEntry(
    std::string const& name, std::vector<std::string> const& arg_names,
    ClientCommand const& command) {
  bool allow_varargs =
      !arg_names.empty() && arg_names.back().find("...") != std::string::npos;
  auto adapter = [=](std::vector<std::string> argv) {
    if ((argv.size() == 1 && argv[0] == "--help") ||
        (allow_varargs ? argv.size() < (arg_names.size() - 1)
                       : argv.size() != arg_names.size())) {
      std::ostringstream os;
      os << name;
      for (auto& a : arg_names) {
        os << " " << a;
      }
      throw Usage{std::move(os).str()};
    }
    auto client = google::cloud::storage::Client::CreateDefaultClient().value();
    command(std::move(client), std::move(argv));
  };
  return {name, std::move(adapter)};
}

}  // namespace examples
}  // namespace storage
}  // namespace cloud
}  // namespace google
