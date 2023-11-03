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

#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include <chrono>
#include <fstream>
#include <sstream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void AccessToken(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{"test-access-token: <project-id> <instance-id> <access-token>"};
  }

  // Create a namespace alias to make the code easier to read.
  namespace gc = ::google::cloud;
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;

  //! [test access token]
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& access_token) {
    auto creds = gc::MakeAccessTokenCredentials(
        access_token, std::chrono::system_clock::now() + std::chrono::hours(1));
    auto options = gc::Options{}.set<gc::UnifiedCredentialsOption>(creds);

    cbta::BigtableTableAdminClient admin(
        cbta::MakeBigtableTableAdminConnection(options));

    google::bigtable::admin::v2::ListTablesRequest r;
    r.set_parent(cbt::InstanceName(project_id, instance_id));
    r.set_view(google::bigtable::admin::v2::Table::NAME_ONLY);
    auto tables = admin.ListTables(std::move(r));
    for (auto& table : tables) {
      if (!table) throw std::move(table).status();
    }
  }
  //! [test access token]
  (argv.at(0), argv.at(1), argv.at(2));
}

void JWTAccessToken(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{
        "test-jwt-access-token <project-id> <instance-id> "
        "<service_account_file_json>"};
  }
  // Create a namespace alias to make the code easier to read.
  namespace gc = ::google::cloud;
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;

  //! [test jwt access token]
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& service_account_file_json) {
    std::ifstream stream(service_account_file_json);
    if (!stream.is_open()) {
      std::ostringstream os;
      os << "JWTAccessToken(" << service_account_file_json
         << "): cannot open upload file source";
      throw std::runtime_error(os.str());
    }
    std::string json_key(std::istreambuf_iterator<char>{stream}, {});

    auto creds = gc::MakeServiceAccountCredentials(std::move(json_key));
    auto options = gc::Options{}.set<gc::UnifiedCredentialsOption>(creds);

    cbta::BigtableTableAdminClient admin(
        cbta::MakeBigtableTableAdminConnection(options));

    google::bigtable::admin::v2::ListTablesRequest r;
    r.set_parent(cbt::InstanceName(project_id, instance_id));
    r.set_view(google::bigtable::admin::v2::Table::NAME_ONLY);
    auto tables = admin.ListTables(std::move(r));
    for (auto& table : tables) {
      if (!table) throw std::move(table).status();
    }
  }
  //! [test jwt access token]
  (argv.at(0), argv.at(1), argv.at(2));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;

  if (!argv.empty()) throw Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_KEY_FILE_JSON",
  });

  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value();
  auto const access_token = google::cloud::internal::GetEnv(
                                "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN")
                                .value();
  auto const credentials_file =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_KEY_FILE_JSON")
          .value();

  AccessToken({project_id, instance_id, access_token});
  JWTAccessToken({project_id, instance_id, credentials_file});
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  google::cloud::bigtable::examples::Commands commands = {
      {"test-access-token", AccessToken},
      {"test-jwt-access-token", JWTAccessToken},
      {"auto", RunAll},
  };

  google::cloud::bigtable::examples::Example example(std::move(commands));
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
