// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/iam/iam_client.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>

namespace {

void ExampleStatusOr(std::vector<std::string> const& argv) {
  //! [example-status-or]
  namespace iam = ::google::cloud::iam;
  [](std::string const& project_id) {
    iam::IAMClient client(iam::MakeIAMConnection());
    int count = 0;
    // The actual type of `service_account` is
    // google::cloud::StatusOr<google::iam::admin::v1::ServiceAccount>, but
    // we expect it'll most often be declared with auto like this.
    for (auto const& sa :
         client.ListServiceAccounts("projects/" + project_id)) {
      // Use `service_account` like a smart pointer; check it before
      // de-referencing
      if (!sa) {
        // `service_account` doesn't contain a value, so `.status()` will
        // contain error info
        std::cerr << sa.status() << "\n";
        break;
      }
      std::cout << "ServiceAccount successfully retrieved: " << sa->name()
                << "\n";
      ++count;
    }
  }
  //! [example-status-or]
  (argv.at(0));
}

void ListServiceAccounts(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "list-service-accounts <project-id>");
  }
  //! [START iam_list_service_accounts] [iam-list-service-accounts]
  namespace iam = google::cloud::iam;
  [](std::string const& project_id) {
    iam::IAMClient client(iam::MakeIAMConnection());
    int count = 0;
    for (auto const& sa :
         client.ListServiceAccounts("projects/" + project_id)) {
      if (!sa) throw std::runtime_error(sa.status().message());
      std::cout << "ServiceAccount successfully retrieved: " << sa->name()
                << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No service accounts found in project: " << project_id
                << "\n";
    }
  }
  //! [END iam_list_service_accounts] [iam-list-service-accounts]
  (argv.at(0));
}

void GetServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "get-service-account <service-account-name>");
  }
  //! [START iam_get_service_account] [iam-get-service-account]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.GetServiceAccount(name);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "ServiceAccount successfully retrieved: "
              << response->DebugString() << "\n";
  }
  //! [END iam_get_service_account] [iam-get-service-account]
  (argv.at(0));
}

void CreateServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw google::cloud::testing_util::Usage(
        "create-service-account <project-id> <account-id> <display-name> "
        "<description>");
  }
  //! [START iam_create_service_account] [iam-create-service-account]
  namespace iam = google::cloud::iam;
  [](std::string const& project_id, std::string const& account_id,
     std::string const& display_name, std::string const& description) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::ServiceAccount service_account;
    service_account.set_display_name(display_name);
    service_account.set_description(description);
    auto response = client.CreateServiceAccount("projects/" + project_id,
                                                account_id, service_account);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "ServiceAccount successfully created: "
              << response->DebugString() << "\n";
  }
  //! [END iam_create_service_account] [iam-create-service-account]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void DeleteServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "delete-service-account <service-account-name>");
  }
  //! [START iam_delete_service_account] [iam-delete-service-account]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.DeleteServiceAccount(name);
    if (!response.ok()) throw std::runtime_error(response.message());
    std::cout << "ServiceAccount successfully deleted.\n";
  }
  //! [END iam_delete_service_account] [iam-delete-service-account]
  (argv.at(0));
}

void ListServiceAccountKeys(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "list-service-account-keys <service-account-name> <key-type> "
        "[<key-type>]*");
  }
  //! [START iam_list_keys] [iam-list-service-account-keys]
  namespace iam = google::cloud::iam;
  [](std::string const& service_account_name,
     std::vector<std::string> const& key_type_labels) {
    iam::IAMClient client(iam::MakeIAMConnection());
    std::vector<google::iam::admin::v1::ListServiceAccountKeysRequest::KeyType>
        key_types;
    for (auto const& type : key_type_labels) {
      if (type == "USER_MANAGED") {
        key_types.push_back(google::iam::admin::v1::
                                ListServiceAccountKeysRequest::USER_MANAGED);
      } else if (type == "SYSTEM_MANAGED") {
        key_types.push_back(google::iam::admin::v1::
                                ListServiceAccountKeysRequest::SYSTEM_MANAGED);
      }
    }
    auto response =
        client.ListServiceAccountKeys(service_account_name, key_types);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "ServiceAccountKeys successfully retrieved: "
              << response->DebugString() << "\n";
  }
  //! [END iam_list_keys] [iam-list-service-account-keys]
  (argv.at(0), {argv.begin() + 1, argv.end()});
}

void GetServiceAccountKey(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "get-service-account-key <service-account-key-name>");
  }
  //! [START iam_get_service_account_key] [iam-get-service-account-key]
  namespace iam = google::cloud::iam;
  [](std::string const& key_name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.GetServiceAccountKey(
        key_name, google::iam::admin::v1::ServiceAccountPublicKeyType::
                      TYPE_X509_PEM_FILE);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "ServiceAccountKey successfully retrieved: "
              << response->DebugString() << "\n";
  }
  //! [END iam_get_service_account_key] [iam-get-service-account-key]
  (argv.at(0));
}

std::string CreateServiceAccountKey(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "create-service-account-key <service-account-name>");
  }
  //! [START iam_create_key] [iam-create-service-account-key]
  namespace iam = google::cloud::iam;
  return [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.CreateServiceAccountKey(
        name,
        google::iam::admin::v1::ServiceAccountPrivateKeyType::
            TYPE_GOOGLE_CREDENTIALS_FILE,
        google::iam::admin::v1::ServiceAccountKeyAlgorithm::KEY_ALG_RSA_2048);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "ServiceAccountKey successfully created: "
              << response->DebugString() << "\n";
    return response->name();
  }
  //! [END iam_create_key] [iam-create-service-account-key]
  (argv.at(0));
}

void DeleteServiceAccountKey(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "delete-service-account-key <service-account-key-name>");
  }
  //! [START iam_delete_key] [iam-delete-service-account-key]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.DeleteServiceAccountKey(name);
    if (!response.ok()) throw std::runtime_error(response.message());
    std::cout << "ServiceAccountKey successfully deleted.\n";
  }
  //! [END iam_delete_key] [iam-delete-service-account-key]
  (argv.at(0));
}

void GetIamPolicy(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage("get-iam-policy <resource-name>");
  }
  //! [START iam_get_policy] [iam-get-iam-policy]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.GetIamPolicy(name);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Policy successfully retrieved: " << response->DebugString()
              << "\n";
  }
  //! [END iam_get_policy] [iam-get-iam-policy]
  (argv.at(0));
}

void SetIamPolicy(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage("set-iam-policy <resource-name>");
  }
  //! [START iam_set_policy] [iam-set-iam-policy]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::v1::Policy policy;
    auto response = client.SetIamPolicy(name, policy);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Policy successfully set: " << response->DebugString() << "\n";
  }
  //! [END iam_set_policy] [iam-set-iam-policy]
  (argv.at(0));
}

void TestIamPermissions(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "test-iam-permissions <resource-name> <permission> [<permission>]*");
  }
  //! [START iam_test_permissions] [iam-test-iam-permissions]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::vector<std::string> const& permissions) {
    iam::IAMClient client(iam::MakeIAMConnection());
    auto response = client.TestIamPermissions(name, permissions);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Permissions successfully tested: " << response->DebugString()
              << "\n";
  }
  //! [END iam_test_permissions] [iam-test-iam-permissions]
  (argv.at(0), {argv.begin() + 1, argv.end()});
}

void QueryGrantableRoles(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "query-grantable-roles <resource-name>");
  }
  //! [START iam_view_grantable_roles] [iam-query-grantable-roles]
  namespace iam = google::cloud::iam;
  [](std::string const& resource) {
    iam::IAMClient client(iam::MakeIAMConnection());
    int count = 0;
    for (auto const& role : client.QueryGrantableRoles(resource)) {
      if (!role) throw std::runtime_error(role.status().message());
      std::cout << "Role successfully retrieved: " << role->name() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No grantable roles found in resource: " << resource << "\n";
    }
  }
  //! [END iam_view_grantable_roles] [iam-query-grantable-roles]
  (argv.at(0));
}

void CreateRole(std::vector<std::string> const& argv) {
  if (argv.size() < 3) {
    throw google::cloud::testing_util::Usage(
        "create-role <parent project> <role_id> <permission> [<permission>]*");
  }
  //! [START iam_create_role] [iam-create-role]
  namespace iam = google::cloud::iam;
  [](std::string const& parent, std::string const& role_id,
     std::vector<std::string> const& included_permissions) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::CreateRoleRequest request;
    request.set_parent("projects/" + parent);
    request.set_role_id(role_id);
    google::iam::admin::v1::Role role;
    role.set_stage(google::iam::admin::v1::Role::GA);
    for (auto const& permission : included_permissions) {
      *role.add_included_permissions() = permission;
    }
    *request.mutable_role() = role;
    auto response = client.CreateRole(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Role successfully created: " << response->DebugString()
              << "\n";
  }
  //! [END iam_create_role] [iam-create-role]
  (argv.at(0), argv.at(1), {argv.begin() + 2, argv.end()});
}

void DeleteRole(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage("delete-role <role-name>");
  }
  //! [START iam_delete_role] [iam-delete-role]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::DeleteRoleRequest request;
    request.set_name(name);
    auto response = client.DeleteRole(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Role successfully deleted: " << response->DebugString()
              << "\n";
  }
  //! [END iam_delete_role] [iam-delete-role]
  (argv.at(0));
}

void DisableServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "disable-service-account <service-account-name>");
  }
  //! [START iam_disable_service_account] [iam-disable-service-account]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::DisableServiceAccountRequest request;
    request.set_name(name);
    auto response = client.DisableServiceAccount(request);
    if (!response.ok()) throw std::runtime_error(response.message());
    std::cout << "ServiceAccount successfully disabled.\n";
  }
  //! [END iam_disable_service_account] [iam-disable-service-account]
  (argv.at(0));
}

void EnableServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "enable-service-account <service-account-name>");
  }
  //! [START iam_enable_service_account] [iam-enable-service-account]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::EnableServiceAccountRequest request;
    request.set_name(name);
    auto response = client.EnableServiceAccount(request);
    if (!response.ok()) throw std::runtime_error(response.message());
    std::cout << "ServiceAccount successfully enabled.\n";
  }
  //! [END iam_enable_service_account] [iam-enable-service-account]
  (argv.at(0));
}

void UpdateRole(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage(
        "update-role <role-name> <new-title>");
  }
  //! [START iam_edit_role] [iam-update-role]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::string const& title) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::UpdateRoleRequest request;
    request.set_name(name);
    google::iam::admin::v1::Role role;
    role.set_title(title);
    google::protobuf::FieldMask update_mask;
    *update_mask.add_paths() = "title";
    *request.mutable_role() = role;
    *request.mutable_update_mask() = update_mask;
    auto response = client.UpdateRole(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Role successfully updated: " << response->DebugString()
              << "\n";
  }
  //! [END iam_edit_role] [iam-update-role]
  (argv.at(0), argv.at(1));
}

void GetRole(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage("get-role <role-name>");
  }
  //! [START iam_get_role] [iam-get-role]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::GetRoleRequest request;
    request.set_name(name);
    auto response = client.GetRole(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Role successfully retrieved: " << response->DebugString()
              << "\n";
  }
  //! [END iam_get_role] [iam-get-role]
  (argv.at(0));
}

void ListRoles(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage("list-roles <parent>");
  }
  //! [START iam_list_roles] [iam-list-roles]
  namespace iam = google::cloud::iam;
  [](std::string const& project) {
    iam::IAMClient client(iam::MakeIAMConnection());
    int count = 0;
    google::iam::admin::v1::ListRolesRequest request;
    request.set_parent(project);
    for (auto const& role : client.ListRoles(request)) {
      if (!role) throw std::runtime_error(role.status().message());
      std::cout << "Roles successfully retrieved: " << role->name() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No roles found in project: " << project << "\n";
    }
  }
  //! [END iam_list_roles] [iam-list-roles]
  (argv.at(0));
}

void QueryTestablePermissions(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage(
        "query-testable-permissions <resource-name>");
  }
  //! [START iam_query_testable_permissions] [iam-query-testable-permissions]
  namespace iam = google::cloud::iam;
  [](std::string const& resource) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::QueryTestablePermissionsRequest request;
    request.set_full_resource_name(resource);
    int count = 0;
    for (auto const& permission : client.QueryTestablePermissions(request)) {
      if (!permission) throw std::runtime_error(permission.status().message());
      std::cout << "Permission successfully retrieved: " << permission->name()
                << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No testable permissions found in resource: " << resource
                << "\n";
    }
  }
  //! [END iam_query_testable_permissions] [iam-query-testable-permissions]
  (argv.at(0));
}

void PatchServiceAccount(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw google::cloud::testing_util::Usage(
        "patch-service-account <service-account-name> <new-display-name>");
  }
  //! [START iam_rename_service_account] [iam-patch-service-account]
  namespace iam = google::cloud::iam;
  [](std::string const& name, std::string const& display_name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::PatchServiceAccountRequest request;
    google::iam::admin::v1::ServiceAccount service_account;
    service_account.set_name(name);
    service_account.set_display_name(display_name);
    google::protobuf::FieldMask update_mask;
    *update_mask.add_paths() = "display_name";
    *request.mutable_service_account() = service_account;
    *request.mutable_update_mask() = update_mask;
    auto response = client.PatchServiceAccount(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "ServiceAccount successfully updated: "
              << response->DebugString() << "\n";
  }
  //! [END iam_rename_service_account] [iam-patch-service-account]
  (argv.at(0), argv.at(1));
}

void UndeleteRole(std::vector<std::string> const& argv) {
  if (argv.size() != 1 || argv.at(0) == "--help") {
    throw google::cloud::testing_util::Usage("undelete-role <role-name>");
  }
  //! [START iam_undelete_role] [iam-undelete-role]
  namespace iam = google::cloud::iam;
  [](std::string const& name) {
    iam::IAMClient client(iam::MakeIAMConnection());
    google::iam::admin::v1::UndeleteRoleRequest request;
    request.set_name(name);
    auto response = client.UndeleteRole(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Role successfully undeleted: " << response->DebugString()
              << "\n";
  }
  //! [END iam_undelete_role] [iam-undelete-role]
  (argv.at(0));
}

bool RunQuotaLimitedSamples() {
  static bool run_quota_limited_samples =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_SAMPLES")
          .value_or("") == "yes";
  return run_quota_limited_samples;
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT",
  });
  auto const service_account_id =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT")
          .value();
  auto const service_account_name =
      absl::StrCat("projects/-/serviceAccounts/", service_account_id);

  ExampleStatusOr({project_id});
  ListServiceAccounts({project_id});
  GetServiceAccount({service_account_name});
  ListServiceAccountKeys(
      {service_account_name, "USER_MANAGED", "SYSTEM_MANAGED"});
  GetIamPolicy({service_account_name});
  TestIamPermissions(
      {service_account_name, "iam.serviceAccounts.getIamPolicy"});
  QueryGrantableRoles(
      {absl::StrCat("//iam.googleapis.com/projects/", project_id,
                    "/serviceAccounts/", service_account_id)});
  ListRoles({absl::StrCat("projects/", project_id)});
  QueryTestablePermissions(
      {absl::StrCat("//iam.googleapis.com/projects/", project_id,
                    "/serviceAccounts/", service_account_id)});

  if (RunQuotaLimitedSamples()) {
    SetIamPolicy({service_account_name});
    CreateServiceAccount({project_id, "sample-account-id", "SampleAccount",
                          "Service Account created during sample execution."});
    auto const sample_service_account_name =
        absl::StrCat("projects/-/serviceAccounts/sample-account-id@",
                     project_id, ".iam.gserviceaccount.com");
    try {
      PatchServiceAccount({sample_service_account_name, "New Name"});
    } catch (std::runtime_error const&) {
      // Service Account may not be usable for up to 60s after creation.
      std::this_thread::sleep_for(std::chrono::seconds(61));
      PatchServiceAccount({sample_service_account_name, "New Name"});
    }
    DisableServiceAccount({sample_service_account_name});
    EnableServiceAccount({sample_service_account_name});
    DeleteServiceAccount({sample_service_account_name});

    auto sample_service_account_key_name =
        CreateServiceAccountKey({service_account_name});
    try {
      GetServiceAccountKey({sample_service_account_key_name});
    } catch (std::runtime_error const&) {
      // Service Account Key may not be usable for up to 60s after creation.
      std::this_thread::sleep_for(std::chrono::seconds(61));
      GetServiceAccountKey({sample_service_account_key_name});
    }
    DeleteServiceAccountKey({sample_service_account_key_name});

    auto role_id = absl::StrCat(
        "iam_sample_role_",
        absl::FormatTime("%Y%m%d%H%M%S", absl::Now(), absl::UTCTimeZone()));
    auto role_name = absl::StrCat("projects/", project_id, "/roles/", role_id);
    CreateRole({project_id, role_id, "iam.serviceAccounts.list"});
    try {
      GetRole({role_name});
    } catch (std::runtime_error const&) {
      // Custom Role may not be usable for up to 60s after creation.
      std::this_thread::sleep_for(std::chrono::seconds(61));
      GetRole({role_name});
    }
    UpdateRole({role_name, "Sample Role Please Ignore"});
    DeleteRole({role_name});
    UndeleteRole({role_name});
    DeleteRole({role_name});
  }
  std::cout << "\nAutoRun done" << std::endl;
}
}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example(
      {{"list-service-accounts", ListServiceAccounts},
       {"get-service-account", GetServiceAccount},
       {"create-service-account", CreateServiceAccount},
       {"delete-service-account", DeleteServiceAccount},
       {"list-service-account-keys", ListServiceAccountKeys},
       {"get-service-account-key", GetServiceAccountKey},
       {"create-service-account-key", CreateServiceAccountKey},
       {"delete-service-account-key", DeleteServiceAccountKey},
       {"get-iam-policy", GetIamPolicy},
       {"set-iam-policy", SetIamPolicy},
       {"test-iam-permissions", TestIamPermissions},
       {"query-grantable-roles", QueryGrantableRoles},
       {"create-role", CreateRole},
       {"delete-role", DeleteRole},
       {"disable-service-account", DisableServiceAccount},
       {"enable-service-account", EnableServiceAccount},
       {"update-role", UpdateRole},
       {"get-role", GetRole},
       {"list-roles", ListRoles},
       {"query-testable-permissions", QueryTestablePermissions},
       {"patch-service-account", PatchServiceAccount},
       {"undelete-role", UndeleteRole},
       {"auto", AutoRun}});
  return example.Run(argc, argv);
}
