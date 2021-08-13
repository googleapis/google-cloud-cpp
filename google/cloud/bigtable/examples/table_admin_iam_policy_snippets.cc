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
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <sstream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void GetIamPolicy(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [get iam policy]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::iam::v1::Policy> policy = admin.GetIamPolicy(table_id);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy for " << table_id << " is\n"
              << policy->DebugString() << "\n";
  }
  //! [get iam policy]
  (std::move(admin), argv.at(0));
}

void SetIamPolicy(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  //! [set iam policy]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& role, std::string const& member) {
    StatusOr<google::iam::v1::Policy> current = admin.GetIamPolicy(table_id);
    if (!current) throw std::runtime_error(current.status().message());
    // This example adds the member to all existing bindings for that role. If
    // there are no such bindings, it adds a new one. This might not be what the
    // user wants, e.g. in case of conditional bindings.
    size_t num_added = 0;
    for (auto& binding : *current->mutable_bindings()) {
      if (binding.role() == role) {
        binding.add_members(member);
        ++num_added;
      }
    }
    if (num_added == 0) {
      *current->add_bindings() = cbt::IamBinding(role, {member});
    }
    StatusOr<google::iam::v1::Policy> policy =
        admin.SetIamPolicy(table_id, *current);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy for " << table_id << " is\n"
              << policy->DebugString() << "\n";
  }
  //! [set iam policy]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void TestIamPermissions(std::vector<std::string> const& argv) {
  if (argv.size() < 4) {
    throw Usage{
        "test-iam-permissions <project-id> <instance-id> <resource-id>"
        " <permission> [<permission>...]"};
  }

  auto it = argv.cbegin();
  auto const project_id = *it++;
  auto const instance_id = *it++;
  auto const resource = *it++;
  std::vector<std::string> const permissions(it, argv.cend());

  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions{}),
      instance_id);

  //! [test iam permissions]

  using ::google::cloud::StatusOr;
  namespace cbt = ::google::cloud::bigtable;

  [](cbt::TableAdmin admin, std::string const& resource,
     std::vector<std::string> const& permissions) {
    StatusOr<std::vector<std::string>> result =
        admin.TestIamPermissions(resource, permissions);
    if (!result) throw std::runtime_error(result.status().message());
    std::cout << "The current user has the following permissions [";
    std::cout << absl::StrJoin(*result, ", ");
    std::cout << "]\n";
  }
  //! [test iam permissions]
  (std::move(admin), std::move(resource), std::move(permissions));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value();
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT")
          .value();

  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions{}),
      instance_id);

  google::cloud::CompletionQueue cq;
  std::thread th([&cq] { cq.Run(); });
  examples::AutoShutdownCQ shutdown(cq, std::move(th));

  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  std::cout << "\nCleaning up old tables" << std::endl;
  google::cloud::bigtable::testing::CleanupStaleTables(admin);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  // This table is actually created and used to test the positive case (e.g.
  // GetTable() and "table does exist")
  auto table_id = google::cloud::bigtable::testing::RandomTableId(generator);

  auto table = admin.CreateTable(
      table_id, cbt::TableConfig(
                    {
                        {"fam", cbt::GcRule::MaxNumVersions(10)},
                        {"foo", cbt::GcRule::MaxNumVersions(3)},
                    },
                    {}));
  if (!table) throw std::runtime_error(table.status().message());

  std::cout << "\nRunning GetIamPolicy() example" << std::endl;
  GetIamPolicy(admin, {table_id});

  std::cout << "\nRunning SetIamPolicy() example" << std::endl;
  SetIamPolicy(admin, {table_id, "roles/bigtable.user",
                       "serviceAccount:" + service_account});

  std::cout << "\nRunning TestIamPermissions() example" << std::endl;
  TestIamPermissions(
      {project_id, instance_id, table_id, "bigtable.tables.get"});

  (void)admin.DeleteTable(table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  namespace examples = ::google::cloud::bigtable::examples;
  examples::Example example({
      examples::MakeCommandEntry("get-iam-policy", {"<table-id>"},
                                 GetIamPolicy),
      examples::MakeCommandEntry(
          "set-iam-policy", {"<table-id>", "<role>", "<member>"}, SetIamPolicy),
      {"test-iam-permissions", TestIamPermissions},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
