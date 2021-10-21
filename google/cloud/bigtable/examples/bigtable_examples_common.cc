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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include <google/protobuf/util/time_util.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

bool UsingEmulator() {
  return !google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST")
              .value_or("")
              .empty();
}

bool RunAdminIntegrationTests() {
  // When using the emulator we can always run the admin integration tests
  if (UsingEmulator()) return true;

  // In production, we run the admin integration tests only on the nightly
  // builds to stay below the quota limits. Only this build should set the
  // following environment variable.
  return google::cloud::internal::GetEnv(
             "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS")
             .value_or("") == "yes";
}

google::cloud::bigtable::examples::Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    TableCommandType const& function) {
  auto command = [=](std::vector<std::string> argv) {
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != 3 + args.size()) {
      std::ostringstream os;
      os << name << " <project-id> <instance-id> <table-id>";
      if (!args.empty()) os << " " << absl::StrJoin(args, " ");
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::Table table(
        google::cloud::bigtable::CreateDefaultDataClient(
            argv[0], argv[1], google::cloud::bigtable::ClientOptions()),
        argv[2]);
    argv.erase(argv.begin(), argv.begin() + 3);
    function(table, argv);
  };
  return {name, command};
}

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAdminCommandType const& function) {
  auto command = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 2;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != args.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id> <instance-id>";
      if (!args.empty()) os << " " << absl::StrJoin(args, " ");
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::TableAdmin table(
        google::cloud::bigtable::CreateDefaultAdminClient(
            argv[0], google::cloud::bigtable::ClientOptions()),
        argv[1]);
    argv.erase(argv.begin(), argv.begin() + kFixedArguments);
    function(table, argv);
  };
  return {name, command};
}

Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    InstanceAdminCommandType const& function) {
  auto command = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 1;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != args.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id>";
      if (!args.empty()) os << " " << absl::StrJoin(args, " ");
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::InstanceAdmin instance(
        google::cloud::bigtable::CreateDefaultInstanceAdminClient(
            argv[0], google::cloud::bigtable::ClientOptions()));
    argv.erase(argv.begin(), argv.begin() + kFixedArguments);
    function(instance, argv);
  };
  return {name, command};
}

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAsyncCommandType const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    std::vector<std::string> const common{"<project-id>", "<instance-id>",
                                          "<table-id>"};
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != common.size() + args.size()) {
      std::ostringstream os;
      os << name;
      for (auto const& a : common) {
        os << " " << a;
      }
      for (auto const& a : args) {
        os << " " << a;
      }
      throw Usage{std::move(os).str()};
    }
    google::cloud::bigtable::Table table(
        google::cloud::bigtable::CreateDefaultDataClient(
            argv[0], argv[1], google::cloud::bigtable::ClientOptions()),
        argv[2]);
    google::cloud::CompletionQueue cq;
    std::thread t([&cq] { cq.Run(); });
    AutoShutdownCQ shutdown(cq, std::move(t));
    argv.erase(argv.begin(), argv.begin() + common.size());
    command(std::move(table), cq, std::move(argv));
  };
  return Commands::value_type{name, std::move(adapter)};
}

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
