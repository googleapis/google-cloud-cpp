// Copyright 2019 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>

namespace {

void GetServiceAccount(google::cloud::storage::Client client,
                       std::vector<std::string> const&) {
  //! [START storage_get_service_account] [get service account]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    StatusOr<gcs::ServiceAccount> account = client.GetServiceAccount();
    if (!account) throw std::runtime_error(account.status().message());

    std::cout << "The service account details are " << *account << "\n";
  }
  //! [END storage_get_service_account] [get service account]
  (std::move(client));
}

void GetServiceAccountForProject(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  //! [get service account for project]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& project_id) {
    StatusOr<gcs::ServiceAccount> account =
        client.GetServiceAccountForProject(project_id);
    if (!account) throw std::runtime_error(account.status().message());

    std::cout << "The service account details for project " << project_id
              << " are " << *account << "\n";
  }
  //! [get service account for project]
  (std::move(client), argv.at(0));
}

void ListHmacKeys(google::cloud::storage::Client client,
                  std::vector<std::string> const&) {
  //! [list hmac keys] [START storage_list_hmac_keys]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    int count = 0;
    gcs::ListHmacKeysReader hmac_keys_list = client.ListHmacKeys();
    for (auto const& key : hmac_keys_list) {
      if (!key) throw std::runtime_error(key.status().message());

      std::cout << "service_account_email = " << key->service_account_email()
                << "\naccess_id = " << key->access_id() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No HMAC keys in default project\n";
    }
  }
  //! [list hmac keys] [END storage_list_hmac_keys]
  (std::move(client));
}

void ListHmacKeysWithServiceAccount(google::cloud::storage::Client client,
                                    std::vector<std::string> const& argv) {
  //! [list hmac keys service account]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& service_account) {
    int count = 0;
    gcs::ListHmacKeysReader hmac_keys_list =
        client.ListHmacKeys(gcs::ServiceAccountFilter(service_account));
    for (auto const& key : hmac_keys_list) {
      if (!key) throw std::runtime_error(key.status().message());

      std::cout << "service_account_email = " << key->service_account_email()
                << "\naccess_id = " << key->access_id() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No HMAC keys for service account " << service_account
                << " in default project\n";
    }
  }
  //! [list hmac keys service account]
  (std::move(client), argv.at(0));
}

std::string CreateHmacKey(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [create hmac key] [START storage_create_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  return [](gcs::Client client, std::string const& service_account_email) {
    StatusOr<std::pair<gcs::HmacKeyMetadata, std::string>> key_info =
        client.CreateHmacKey(service_account_email);
    if (!key_info) throw std::runtime_error(key_info.status().message());

    std::cout << "The base64 encoded secret is: " << key_info->second
              << "\nDo not miss that secret, there is no API to recover it."
              << "\nThe HMAC key metadata is: " << key_info->first << "\n";
    return key_info->first.access_id();
  }
  //! [create hmac key] [END storage_create_hmac_key]
  (std::move(client), argv.at(0));
}

std::string CreateHmacKeyForProject(google::cloud::storage::Client client,
                                    std::vector<std::string> const& argv) {
  //! [create hmac key project]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  return [](gcs::Client client, std::string const& project_id,
            std::string const& service_account_email) {
    StatusOr<std::pair<gcs::HmacKeyMetadata, std::string>> hmac_key_details =
        client.CreateHmacKey(service_account_email,
                             gcs::OverrideDefaultProject(project_id));
    if (!hmac_key_details) {
      throw std::runtime_error(hmac_key_details.status().message());
    }

    std::cout << "The base64 encoded secret is: " << hmac_key_details->second
              << "\nDo not miss that secret, there is no API to recover it."
              << "\nThe HMAC key metadata is: " << hmac_key_details->first
              << "\n";
    return hmac_key_details->first.access_id();
  }
  //! [create hmac key project]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteHmacKey(google::cloud::storage::Client client,
                   std::vector<std::string> const& argv) {
  //! [delete hmac key] [START storage_delete_hmac_key]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& access_id) {
    google::cloud::Status status = client.DeleteHmacKey(access_id);
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The key is deleted, though it may still appear"
              << " in ListHmacKeys() results.\n";
  }
  //! [delete hmac key] [END storage_delete_hmac_key]
  (std::move(client), argv.at(0));
}

void GetHmacKey(google::cloud::storage::Client client,
                std::vector<std::string> const& argv) {
  //! [get hmac key] [START storage_get_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& access_id) {
    StatusOr<gcs::HmacKeyMetadata> hmac_key = client.GetHmacKey(access_id);
    if (!hmac_key) throw std::runtime_error(hmac_key.status().message());

    std::cout << "The HMAC key metadata is: " << *hmac_key << "\n";
  }
  //! [get hmac key] [END storage_get_hmac_key]
  (std::move(client), argv.at(0));
}

void UpdateHmacKey(google::cloud::storage::Client client,
                   std::vector<std::string> const& argv) {
  //! [update hmac key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& access_id,
     std::string const& state) {
    StatusOr<gcs::HmacKeyMetadata> updated = client.UpdateHmacKey(
        access_id, gcs::HmacKeyMetadata().set_state(std::move(state)));
    if (!updated) throw std::runtime_error(updated.status().message());

    std::cout << "The updated HMAC key metadata is: " << *updated << "\n";
  }
  //! [update hmac key]
  (std::move(client), argv.at(0), argv.at(1));
}

void ActivateHmacKey(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [START storage_activate_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& access_id) {
    StatusOr<gcs::HmacKeyMetadata> updated = client.UpdateHmacKey(
        access_id,
        gcs::HmacKeyMetadata().set_state(gcs::HmacKeyMetadata::state_active()));
    if (!updated) throw std::runtime_error(updated.status().message());

    if (updated->state() != gcs::HmacKeyMetadata::state_active()) {
      throw std::runtime_error(
          "The HMAC key is NOT active, this is unexpected");
    }
    std::cout << "The HMAC key is now active\nFull metadata: " << *updated
              << "\n";
  }
  //! [END storage_activate_hmac_key]
  (std::move(client), argv.at(0));
}

void DeactivateHmacKey(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [START storage_deactivate_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& access_id) {
    StatusOr<gcs::HmacKeyMetadata> updated = client.UpdateHmacKey(
        access_id, gcs::HmacKeyMetadata().set_state(
                       gcs::HmacKeyMetadata::state_inactive()));
    if (!updated) throw std::runtime_error(updated.status().message());

    if (updated->state() != gcs::HmacKeyMetadata::state_inactive()) {
      throw std::runtime_error("The HMAC key is active, this is unexpected");
    }
    std::cout << "The HMAC key is now inactive\nFull metadata: " << *updated
              << "\n";
  }
  //! [END storage_deactivate_hmac_key]
  (std::move(client), argv.at(0));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT")
          .value();
  auto client = gcs::Client::CreateDefaultClient().value();

  std::cout << "\nRunning GetServiceAccountForProject() example" << std::endl;
  GetServiceAccountForProject(client, {project_id});

  std::cout << "\nRunning GetServiceAccount() example" << std::endl;
  GetServiceAccount(client, {});

  std::cout << "\nRunning ListHmacKeys() example [1]" << std::endl;
  ListHmacKeys(client, {});

  std::cout << "\nRunning ListHmacKeysWithServiceAccount() example [1]"
            << std::endl;
  ListHmacKeysWithServiceAccount(client, {service_account});

  auto const key_info =
      client
          .CreateHmacKey(service_account,
                         gcs::OverrideDefaultProject(project_id))
          .value();

  std::cout << "\nRunning CreateHmacKey() example" << std::endl;
  auto const hmac_access_id = CreateHmacKey(client, {service_account});

  std::cout << "\nRunning CreateHmacKeyForProject() example" << std::endl;
  auto const project_hmac_access_id =
      CreateHmacKeyForProject(client, {project_id, service_account});

  std::cout << "\nRunning ListHmacKeys() example [2]" << std::endl;
  ListHmacKeys(client, {});

  std::cout << "\nRunning ListHmacKeysWithServiceAccount() example [2]"
            << std::endl;
  ListHmacKeysWithServiceAccount(client, {service_account});

  std::cout << "\nRunning GetHmacKey() example" << std::endl;
  GetHmacKey(client, {key_info.first.access_id()});

  std::cout << "\nRunning UpdateHmacKey() example" << std::endl;
  UpdateHmacKey(client, {key_info.first.access_id(), "INACTIVE"});

  std::cout << "\nRunning ActivateHmacKey() example" << std::endl;
  ActivateHmacKey(client, {key_info.first.access_id()});

  std::cout << "\nRunning DeactivateHmacKey() example" << std::endl;
  DeactivateHmacKey(client, {key_info.first.access_id()});

  std::cout << "\nRunning DeleteHmacKey() example" << std::endl;
  DeleteHmacKey(client, {key_info.first.access_id()});

  for (auto const& access_id : {project_hmac_access_id, hmac_access_id}) {
    (void)client.UpdateHmacKey(access_id,
                               gcs::HmacKeyMetadata().set_state(
                                   gcs::HmacKeyMetadata::state_inactive()));
    (void)client.DeleteHmacKey(access_id);
  }
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("get-service-account", {},
                                   GetServiceAccount),
      examples::CreateCommandEntry("get-service-account-for-project",
                                   {"<project-id>"},
                                   GetServiceAccountForProject),
      examples::CreateCommandEntry("list-hmac-keys", {}, ListHmacKeys),
      examples::CreateCommandEntry("list-hmac-keys-with-service-account",
                                   {"<service-account>"},
                                   ListHmacKeysWithServiceAccount),
      examples::CreateCommandEntry("create-hmac-key",
                                   {"<service-account-email>"}, CreateHmacKey),
      examples::CreateCommandEntry("create-hmac-key-for-project",
                                   {"<project-id>", "<service-account-email>"},
                                   CreateHmacKeyForProject),
      examples::CreateCommandEntry("delete-hmac-key", {"<access-id>"},
                                   DeleteHmacKey),
      examples::CreateCommandEntry("get-hmac-key", {"<access-id>"}, GetHmacKey),
      examples::CreateCommandEntry("update-hmac-key",
                                   {"<access-id>", "<state>"}, UpdateHmacKey),
      examples::CreateCommandEntry("activate-hmac-key", {"<access-id>"},
                                   ActivateHmacKey),
      examples::CreateCommandEntry("deactivate-hmac-key", {"<access-id>"},
                                   DeactivateHmacKey),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
