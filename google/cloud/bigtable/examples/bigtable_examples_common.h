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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_BIGTABLE_EXAMPLES_COMMON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_BIGTABLE_EXAMPLES_COMMON_H

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

using ::google::cloud::testing_util::CheckEnvironmentVariablesAreSet;
using ::google::cloud::testing_util::Commands;
using ::google::cloud::testing_util::CommandType;
using ::google::cloud::testing_util::Example;
using ::google::cloud::testing_util::Usage;

std::string TablePrefix(std::string const& prefix,
                        std::chrono::system_clock::time_point tp);
std::string RandomTableId(std::string const& prefix,
                          google::cloud::internal::DefaultPRNG& generator);
void CleanupOldTables(std::string const& prefix,
                      google::cloud::bigtable::TableAdmin admin);

std::string InstancePrefix(std::string const& prefix,
                           std::chrono::system_clock::time_point tp);
std::string RandomInstanceId(std::string const& prefix,
                             google::cloud::internal::DefaultPRNG& generator);
void CleanupOldInstances(std::string const& prefix,
                         google::cloud::bigtable::InstanceAdmin admin);

std::string RandomClusterId(std::string const& prefix,
                            google::cloud::internal::DefaultPRNG& generator);

bool UsingEmulator();
bool RunAdminIntegrationTests();

class AutoShutdownCQ {
 public:
  AutoShutdownCQ(google::cloud::CompletionQueue cq, std::thread th)
      : cq_(std::move(cq)), th_(std::move(th)) {}
  ~AutoShutdownCQ() {
    cq_.Shutdown();
    th_.join();
  }

  AutoShutdownCQ(AutoShutdownCQ&&) = delete;
  AutoShutdownCQ& operator=(AutoShutdownCQ&&) = delete;

 private:
  google::cloud::CompletionQueue cq_;
  std::thread th_;
};

using TableCommandType = std::function<void(google::cloud::bigtable::Table,
                                            std::vector<std::string>)>;

google::cloud::bigtable::examples::Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    TableCommandType const& function);

using TableAdminCommandType = std::function<void(
    google::cloud::bigtable::TableAdmin, std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAdminCommandType const& function);

using InstanceAdminCommandType = std::function<void(
    google::cloud::bigtable::InstanceAdmin, std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      InstanceAdminCommandType const& function);

using TableAsyncCommandType = std::function<void(google::cloud::bigtable::Table,
                                                 google::cloud::CompletionQueue,
                                                 std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAsyncCommandType const& command);

using InstanceAdminAsyncCommandType = std::function<void(
    google::cloud::bigtable::InstanceAdmin, google::cloud::CompletionQueue,
    std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    InstanceAdminAsyncCommandType const& command);

using TableAdminAsyncCommandType = std::function<void(
    google::cloud::bigtable::TableAdmin, google::cloud::CompletionQueue,
    std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(
    std::string const& name, std::vector<std::string> const& args,
    TableAdminAsyncCommandType const& command);

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_BIGTABLE_EXAMPLES_COMMON_H
