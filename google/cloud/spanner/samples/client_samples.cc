// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <fstream>
#include <string>
#include <vector>

namespace {

void SetClientEndpoint(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "set-client-endpoint <project-id> <instance-id> <database-id>"};
  }
  //! [set-client-endpoint]
  namespace spanner = ::google::cloud::spanner;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    auto options = google::cloud::Options{}.set<google::cloud::EndpointOption>(
        "private.googleapis.com");
    return spanner::Client(spanner::MakeConnection(
        spanner::Database(project_id, instance_id, database_id), options));
  }
  //! [set-client-endpoint]
  (argv.at(0), argv.at(1), argv.at(2));
}

void WithServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw google::cloud::testing_util::Usage{
        "with-service-account <project-id> <instance-id> <database-id> "
        "<keyfile>"};
  }
  //! [with-service-account]
  namespace spanner = ::google::cloud::spanner;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id, std::string const& keyfile) {
    auto is = std::ifstream(keyfile);
    is.exceptions(std::ios::badbit);
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    auto options =
        google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeServiceAccountCredentials(contents));
    return spanner::Client(spanner::MakeConnection(
        spanner::Database(project_id, instance_id, database_id), options));
  }
  //! [with-service-account]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void AutoRun(std::vector<std::string> const& argv) {
  using ::google::cloud::internal::GetEnv;
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const emulator = GetEnv("SPANNER_EMULATOR_HOST").has_value();
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const instance_id =
      google::cloud::spanner_testing::RandomInstanceName(generator);
  auto const database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);

  std::cout << "\nRunning SetClientEndpoint() example" << std::endl;
  SetClientEndpoint({project_id, instance_id, database_id});

  if (!emulator) {
    // On the emulator this blocks, as the emulator does not support credentials
    // that require SSL.
    examples::CheckEnvironmentVariablesAreSet({
        "GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE",
    });
    auto const keyfile =
        GetEnv("GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE").value();
    std::cout << "\nRunning WithServiceAccount() example" << std::endl;
    WithServiceAccount({project_id, instance_id, database_id, keyfile});
  }

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"set-client-endpoint", SetClientEndpoint},
      {"with-service-account", WithServiceAccount},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
