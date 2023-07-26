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

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <fstream>
#include <string>
#include <vector>

namespace {

void TableSetEndpoint(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 3) {
    throw examples::Usage{
        "table-set-endpoint <project-id> <instance-id> <table-id>"};
  }
  //! [table-set-endpoint]
  namespace bigtable = ::google::cloud::bigtable;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& table_id) {
    auto options = google::cloud::Options{}.set<google::cloud::EndpointOption>(
        "private.googleapis.com");
    auto resource = bigtable::TableResource(project_id, instance_id, table_id);
    return bigtable::Table(bigtable::MakeDataConnection(options), resource);
  }
  //! [table-set-endpoint]
  (argv.at(0), argv.at(1), argv.at(2));
}

void SetRetryPolicy(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw google::cloud::testing_util::Usage{
        "set-retry-policy <project-id> <instance-id> <table-id>"};
  }
  //! [set-retry-policy]
  namespace cbt = google::cloud::bigtable;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& table_id) {
    auto options = google::cloud::Options{}
                       .set<cbt::IdempotentMutationPolicyOption>(
                           cbt::AlwaysRetryMutationPolicy().clone())
                       .set<cbt::DataRetryPolicyOption>(
                           cbt::DataLimitedErrorCountRetryPolicy(3).clone())
                       .set<cbt::DataBackoffPolicyOption>(
                           google::cloud::ExponentialBackoffPolicy(
                               /*initial_delay=*/std::chrono::milliseconds(200),
                               /*maximum_delay=*/std::chrono::seconds(45),
                               /*scaling=*/2.0)
                               .clone());
    auto connection = cbt::MakeDataConnection(options);

    auto const table_name =
        cbt::TableResource(project_id, instance_id, table_id);
    // c1 and c2 share the same retry policies
    auto c1 = cbt::Table(connection, table_name);
    auto c2 = cbt::Table(connection, table_name);

    // You can override any of the policies in a new client. This new client
    // will share the policies from c1 (or c2) *except* from the retry policy.
    auto c3 = cbt::Table(
        connection, table_name,
        google::cloud::Options{}.set<cbt::DataRetryPolicyOption>(
            cbt::DataLimitedTimeRetryPolicy(std::chrono::minutes(5)).clone()));

    // You can also override the policies in a single call. In this case, we
    // allow no retries.
    auto result =
        c3.ReadRow("my-key", cbt::Filter::PassAllFilter(),
                   google::cloud::Options{}.set<cbt::DataRetryPolicyOption>(
                       cbt::DataLimitedErrorCountRetryPolicy(0).clone()));
    (void)result;  // ignore errors in this example
  }
  //! [set-retry-policy]
  (argv.at(0), argv.at(1), argv.at(2));
}

void TableWithServiceAccount(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 4) {
    throw examples::Usage{
        "table-with-service-account <project-id> <instance-id> <table-id> "
        "<keyfile>"};
  }
  //! [table-with-service-account]
  namespace bigtable = ::google::cloud::bigtable;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& table_id, std::string const& keyfile) {
    auto is = std::ifstream(keyfile);
    is.exceptions(std::ios::badbit);
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    auto options =
        google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeServiceAccountCredentials(contents));
    auto resource = bigtable::TableResource(project_id, instance_id, table_id);
    return bigtable::Table(bigtable::MakeDataConnection(options), resource);
  }
  //! [table-with-service-account]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void AutoRun(std::vector<std::string> const& argv) {
  using ::google::cloud::internal::GetEnv;
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE",
  });
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const keyfile =
      GetEnv("GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE").value();

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const instance_id =
      google::cloud::bigtable::testing::RandomInstanceId(generator);
  auto const table_id =
      google::cloud::bigtable::testing::RandomTableId(generator);

  std::cout << "\nRunning TableSetEndpoint() sample" << std::endl;
  TableSetEndpoint({project_id, instance_id, table_id});

  std::cout << "\nRunning TableWithServiceAccount() sample" << std::endl;
  TableWithServiceAccount({project_id, instance_id, table_id, keyfile});

  std::cout << "\nRunning SetRetryPolicy() sample" << std::endl;
  SetRetryPolicy({project_id, instance_id, table_id});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"table-set-endpoint", TableSetEndpoint},
      {"set-retry-policy", SetRetryPolicy},
      {"table-with-service-account", TableWithServiceAccount},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
