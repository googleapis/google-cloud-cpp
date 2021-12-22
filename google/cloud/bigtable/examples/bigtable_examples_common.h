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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_BIGTABLE_EXAMPLES_COMMON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_BIGTABLE_EXAMPLES_COMMON_H

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <chrono>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace examples {

using ::google::cloud::testing_util::CheckEnvironmentVariablesAreSet;
using ::google::cloud::testing_util::Commands;
using ::google::cloud::testing_util::CommandType;
using ::google::cloud::testing_util::Example;
using ::google::cloud::testing_util::Usage;

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

using TableAdminCommandType =
    std::function<void(google::cloud::bigtable_admin::BigtableTableAdminClient,
                       std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAdminCommandType const& function);

using InstanceAdminCommandType = std::function<void(
    bigtable_admin::BigtableInstanceAdminClient, std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      InstanceAdminCommandType const& function);

using TableAsyncCommandType = std::function<void(google::cloud::bigtable::Table,
                                                 google::cloud::CompletionQueue,
                                                 std::vector<std::string>)>;

Commands::value_type MakeCommandEntry(std::string const& name,
                                      std::vector<std::string> const& args,
                                      TableAsyncCommandType const& command);

}  // namespace examples
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EXAMPLES_BIGTABLE_EXAMPLES_COMMON_H
