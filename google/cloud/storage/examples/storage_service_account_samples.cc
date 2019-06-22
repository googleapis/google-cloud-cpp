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
#include <functional>
#include <iostream>
#include <map>
#include <sstream>

namespace {
struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void GetServiceAccount(google::cloud::storage::Client client, int& argc,
                       char*[]) {
  if (argc != 1) {
    throw Usage{"get-service-account"};
  }
  //! [get service account]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    StatusOr<gcs::ServiceAccount> service_account_details =
        client.GetServiceAccount();

    if (!service_account_details) {
      throw std::runtime_error(service_account_details.status().message());
    }
    std::cout << "The service account details are " << *service_account_details
              << "\n";
  }
  //! [get service account]
  (std::move(client));
}

void GetServiceAccountForProject(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-service-account-for-project <project-id>"};
  }
  auto project_id = ConsumeArg(argc, argv);
  //! [get service account for project]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string project_id) {
    StatusOr<gcs::ServiceAccount> service_account_details =
        client.GetServiceAccountForProject(project_id);

    if (!service_account_details) {
      throw std::runtime_error(service_account_details.status().message());
    }

    std::cout << "The service account details for project " << project_id
              << " are " << *service_account_details << "\n";
  }
  //! [get service account for project]
  (std::move(client), project_id);
}

void ListHmacKeys(google::cloud::storage::Client client, int& argc, char*[]) {
  if (argc != 1) {
    throw Usage{"list-hmac-keys"};
  }
  //! [list hmac keys] [START storage_list_hmac_keys]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    int count = 0;
    gcs::ListHmacKeysReader hmac_keys_list = client.ListHmacKeys();
    for (auto&& hmac_key_metadata : hmac_keys_list) {
      if (!hmac_key_metadata) {
        throw std::runtime_error(hmac_key_metadata.status().message());
      }
      std::cout << "service_account_email = "
                << hmac_key_metadata->service_account_email()
                << "\naccess_id = " << hmac_key_metadata->access_id() << "\n";
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
                                    int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-hmac-keys-with-service-account <service-account>"};
  }
  //! [list hmac keys service account]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string service_account) {
    int count = 0;
    gcs::ListHmacKeysReader hmac_keys_list =
        client.ListHmacKeys(gcs::ServiceAccountFilter(service_account));
    for (auto&& hmac_key_metadata : hmac_keys_list) {
      if (!hmac_key_metadata) {
        throw std::runtime_error(hmac_key_metadata.status().message());
      }
      std::cout << "service_account_email = "
                << hmac_key_metadata->service_account_email()
                << "\naccess_id = " << hmac_key_metadata->access_id() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No HMAC keys for service account " << service_account
                << " in default project\n";
    }
  }
  //! [list hmac keys service account]
  (std::move(client), argv[1]);
}

void CreateHmacKey(google::cloud::storage::Client client, int& argc,
                   char* argv[]) {
  if (argc != 2) {
    throw Usage{"create-hmac-key <service-account-email>"};
  }
  //! [create hmac key] [START storage_create_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string service_account_email) {
    StatusOr<std::pair<gcs::HmacKeyMetadata, std::string>> hmac_key_details =
        client.CreateHmacKey(service_account_email);

    if (!hmac_key_details) {
      throw std::runtime_error(hmac_key_details.status().message());
    }
    std::cout << "The base64 encoded secret is: " << hmac_key_details->second
              << "\nDo not miss that secret, there is no API to recover it."
              << "\nThe HMAC key metadata is: " << hmac_key_details->first
              << "\n";
  }
  //! [create hmac key] [END storage_create_hmac_key]
  (std::move(client), argv[1]);
}

void CreateHmacKeyForProject(google::cloud::storage::Client client, int& argc,
                             char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "create-hmac-key-for-project <project-id> <service-account-email>"};
  }
  //! [create hmac key project]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string project_id,
     std::string service_account_email) {
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
  }
  //! [create hmac key project]
  (std::move(client), argv[1], argv[2]);
}

void DeleteHmacKey(google::cloud::storage::Client client, int& argc,
                   char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-hmac-key <access-id>"};
  }
  //! [delete hmac key] [START storage_delete_hmac_key]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string access_id) {
    google::cloud::Status status = client.DeleteHmacKey(access_id);

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "The key is deleted, though it may still appear"
              << " in ListHmacKeys() results.\n";
  }
  //! [delete hmac key] [END storage_delete_hmac_key]
  (std::move(client), argv[1]);
}

void GetHmacKey(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-hmac-key <access-id>"};
  }
  //! [get hmac key] [START storage_get_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string access_id) {
    StatusOr<gcs::HmacKeyMetadata> hmac_key_details =
        client.GetHmacKey(access_id);

    if (!hmac_key_details) {
      throw std::runtime_error(hmac_key_details.status().message());
    }
    std::cout << "The HMAC key metadata is: " << *hmac_key_details << "\n";
  }
  //! [get hmac key] [END storage_get_hmac_key]
  (std::move(client), argv[1]);
}

void UpdateHmacKey(google::cloud::storage::Client client, int& argc,
                   char* argv[]) {
  if (argc != 3) {
    throw Usage{"update-hmac-key <access-id> <state>"};
  }
  //! [update hmac key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string access_id, std::string state) {
    StatusOr<gcs::HmacKeyMetadata> current_metadata =
        client.GetHmacKey(access_id);
    if (!current_metadata) {
      throw std::runtime_error(current_metadata.status().message());
    }

    StatusOr<gcs::HmacKeyMetadata> updated_metadata = client.UpdateHmacKey(
        access_id, gcs::HmacKeyMetadata().set_state(std::move(state)));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }
    std::cout << "The updated HMAC key metadata is: " << *updated_metadata
              << "\n";
  }
  //! [update hmac key]
  (std::move(client), argv[1], argv[2]);
}

void ActivateHmacKey(google::cloud::storage::Client client, int& argc,
                     char* argv[]) {
  if (argc != 2) {
    throw Usage{"activate-hmac-key <access-id>"};
  }
  //! [START storage_activate_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string access_id) {
    StatusOr<gcs::HmacKeyMetadata> updated_metadata = client.UpdateHmacKey(
        access_id,
        gcs::HmacKeyMetadata().set_state(gcs::HmacKeyMetadata::state_active()));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }
    if (updated_metadata->state() != gcs::HmacKeyMetadata::state_active()) {
      throw std::runtime_error(
          "The HMAC key is NOT active, this is unexpected");
    }
    std::cout << "The HMAC key is now active\nFull metadata: "
              << *updated_metadata << "\n";
  }
  //! [END storage_activate_hmac_key]
  (std::move(client), argv[1]);
}

void DeactivateHmacKey(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 2) {
    throw Usage{"deactivate-hmac-key <access-id>"};
  }
  //! [START storage_deactivate_hmac_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string access_id) {
    StatusOr<gcs::HmacKeyMetadata> updated_metadata = client.UpdateHmacKey(
        access_id, gcs::HmacKeyMetadata().set_state(
                       gcs::HmacKeyMetadata::state_inactive()));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }
    if (updated_metadata->state() != gcs::HmacKeyMetadata::state_inactive()) {
      throw std::runtime_error("The HMAC key is active, this is unexpected");
    }
    std::cout << "The HMAC key is now inactive\nFull metadata: "
              << *updated_metadata << "\n";
  }
  //! [END storage_deactivate_hmac_key]
  (std::move(client), argv[1]);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << "\n";
    return 1;
  }

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"get-service-account", &GetServiceAccount},
      {"get-service-account-for-project", &GetServiceAccountForProject},
      {"list-hmac-keys", &ListHmacKeys},
      {"list-hmac-keys-with-service-account", &ListHmacKeysWithServiceAccount},
      {"create-hmac-key-for-project", &CreateHmacKeyForProject},
      {"create-hmac-key", &CreateHmacKey},
      {"delete-hmac-key", &DeleteHmacKey},
      {"get-hmac-key", &GetHmacKey},
      {"update-hmac-key", &UpdateHmacKey},
      {"activate-hmac-key", &ActivateHmacKey},
      {"deactivate-hmac-key", &DeactivateHmacKey},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 0;
      kv.second(*client, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
    } catch (...) {
      // ignore other exceptions.
    }
  }

  if (argc < 2) {
    PrintUsage(argc, argv, "Missing command");
    return 1;
  }

  std::string const command = ConsumeArg(argc, argv);
  auto it = commands.find(command);
  if (commands.end() == it) {
    PrintUsage(argc, argv, "Unknown command: " + command);
    return 1;
  }

  it->second(*client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
