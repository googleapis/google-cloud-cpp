// Copyright 2018 Google LLC
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

void GetBucketIamPolicy(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-iam-policy <bucket_name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket iam policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<google::cloud::IamPolicy> policy =
        client.GetBucketIamPolicy(bucket_name);

    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }

    std::cout << "The IAM policy for bucket " << bucket_name << " is "
              << *policy << "\n";
  }
  //! [get bucket iam policy]
  (std::move(client), bucket_name);
}

void NativeGetBucketIamPolicy(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc != 2) {
    throw Usage{"native-get-bucket-iam-policy <bucket_name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [native get bucket iam policy] [START storage_view_bucket_iam_members]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    auto policy = client.GetNativeBucketIamPolicy(bucket_name);

    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }

    std::cout << "The IAM policy for bucket " << bucket_name << " is "
              << *policy << "\n";
  }
  //! [native get bucket iam policy] [END storage_view_bucket_iam_members]
  (std::move(client), bucket_name);
}

void AddBucketIamMember(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 4) {
    throw Usage{"add-bucket-iam-member <bucket_name> <role> <member>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  auto member = ConsumeArg(argc, argv);
  //! [add bucket iam member]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string role,
     std::string member) {
    StatusOr<google::cloud::IamPolicy> policy =
        client.GetBucketIamPolicy(bucket_name);

    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }

    policy->bindings.AddMember(role, member);

    StatusOr<google::cloud::IamPolicy> updated_policy =
        client.SetBucketIamPolicy(bucket_name, *policy);

    if (!updated_policy) {
      throw std::runtime_error(updated_policy.status().message());
    }

    std::cout << "Updated IAM policy bucket " << bucket_name
              << ". The new policy is " << *updated_policy << "\n";
  }
  //! [add bucket iam member]
  (std::move(client), bucket_name, role, member);
}

void NativeAddBucketIamMember(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc != 4) {
    throw Usage{"native-add-bucket-iam-member <bucket_name> <role> <member>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  auto member = ConsumeArg(argc, argv);
  //! [native add bucket iam member] [START storage_add_bucket_iam_member]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string role,
     std::string member) {
    auto policy = client.GetNativeBucketIamPolicy(bucket_name);

    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }

    for (auto& binding : policy->bindings()) {
      if (binding.role() != role) {
        continue;
      }
      auto& members = binding.members();
      if (std::find(members.begin(), members.end(), member) == members.end()) {
        members.emplace_back(member);
      }
    }

    auto updated_policy = client.SetNativeBucketIamPolicy(bucket_name, *policy);

    if (!updated_policy) {
      throw std::runtime_error(updated_policy.status().message());
    }

    std::cout << "Updated IAM policy bucket " << bucket_name
              << ". The new policy is " << *updated_policy << "\n";
  }
  //! [native add bucket iam member] [END storage_add_bucket_iam_member]
  (std::move(client), bucket_name, role, member);
}

void RemoveBucketIamMember(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 4) {
    throw Usage{"remove-bucket-iam-member <bucket_name> <role> <member>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  auto member = ConsumeArg(argc, argv);
  //! [remove bucket iam member]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string role,
     std::string member) {
    StatusOr<google::cloud::IamPolicy> policy =
        client.GetBucketIamPolicy(bucket_name);
    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }

    policy->bindings.RemoveMember(role, member);

    StatusOr<google::cloud::IamPolicy> updated_policy =
        client.SetBucketIamPolicy(bucket_name, *policy);

    if (!updated_policy) {
      throw std::runtime_error(updated_policy.status().message());
    }

    std::cout << "Updated IAM policy bucket " << bucket_name
              << ". The new policy is " << *updated_policy << "\n";
  }
  //! [remove bucket iam member]
  (std::move(client), bucket_name, role, member);
}

void NativeRemoveBucketIamMember(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "native-remove-bucket-iam-member <bucket_name> <role> <member>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto role = ConsumeArg(argc, argv);
  auto member = ConsumeArg(argc, argv);
  //! [native remove bucket iam member] [START storage_remove_bucket_iam_member]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string role,
     std::string member) {
    auto policy = client.GetNativeBucketIamPolicy(bucket_name);
    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }

    std::vector<google::cloud::storage::NativeIamBinding> updated_bindings;
    for (auto& binding : policy->bindings()) {
      auto& members = binding.members();
      if (binding.role() == role) {
        members.erase(std::remove(members.begin(), members.end(), member),
                      members.end());
      }
      if (!members.empty()) {
        updated_bindings.emplace_back(std::move(binding));
      }
    }
    policy->bindings() = std::move(updated_bindings);

    auto updated_policy = client.SetNativeBucketIamPolicy(bucket_name, *policy);

    if (!updated_policy) {
      throw std::runtime_error(updated_policy.status().message());
    }

    std::cout << "Updated IAM policy bucket " << bucket_name
              << ". The new policy is " << *updated_policy << "\n";
  }
  //! [native remove bucket iam member] [END storage_remove_bucket_iam_member]
  (std::move(client), bucket_name, role, member);
}

void TestBucketIamPermissions(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc < 3) {
    throw Usage{
        "test-bucket-iam-permissions <bucket_name> <permission>"
        " [permission ...]"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  std::vector<std::string> permissions;
  while (argc >= 2) {
    permissions.emplace_back(ConsumeArg(argc, argv));
  }
  //! [test bucket iam permissions]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name,
     std::vector<std::string> permissions) {
    StatusOr<std::vector<std::string>> actual_permissions =
        client.TestBucketIamPermissions(bucket_name, permissions);

    if (!actual_permissions) {
      throw std::runtime_error(actual_permissions.status().message());
    }

    if (actual_permissions->empty()) {
      std::cout << "The caller does not hold any of the tested permissions the"
                << " bucket " << bucket_name << "\n";
      return;
    }

    std::cout << "The caller is authorized for the following permissions on "
              << bucket_name << ": ";
    for (auto const& permission : *actual_permissions) {
      std::cout << "\n    " << permission;
    }
    std::cout << "\n";
  }
  //! [test bucket iam permissions]
  (std::move(client), bucket_name, permissions);
}

void SetBucketPublicIam(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 2) {
    throw Usage{"set-bucket-public-iam <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  // [START storage_set_bucket_public_iam]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<google::cloud::IamPolicy> current_policy =
        client.GetBucketIamPolicy(bucket_name);

    if (!current_policy) {
      throw std::runtime_error(current_policy.status().message());
    }

    current_policy->bindings.AddMember("roles/storage.objectViewer",
                                       "allUsers");

    // Update the policy. Note the use of `gcs::IfMatchEtag` to implement
    // optimistic concurrency control.
    StatusOr<google::cloud::IamPolicy> updated_policy =
        client.SetBucketIamPolicy(bucket_name, *current_policy,
                                  gcs::IfMatchEtag(current_policy->etag));

    if (!updated_policy) {
      throw std::runtime_error(current_policy.status().message());
    }

    auto role = updated_policy->bindings.find("roles/storage.objectViewer");
    if (role == updated_policy->bindings.end()) {
      std::cout << "Cannot find 'roles/storage.objectViewer' in the updated"
                << " policy. This can happen if another application updates"
                << " the IAM policy at the same time. Please retry the"
                << " operation.\n";
      return;
    }
    auto member = role->second.find("allUsers");
    if (member == role->second.end()) {
      std::cout << "'allUsers' is not a member of the"
                << " 'roles/storage.objectViewer' role in the updated"
                << " policy. This can happen if another application updates"
                << " the IAM policy at the same time. Please retry the"
                << " operation.\n";
      return;
    }
    std::cout << "IamPolicy successfully updated for bucket " << bucket_name
              << '\n';
  }
  // [END storage_set_bucket_public_iam]
  (std::move(client), bucket_name);
}

void NativeSetBucketPublicIam(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc != 2) {
    throw Usage{"set-bucket-public-iam <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  // [START native storage_set_bucket_public_iam]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    auto current_policy = client.GetNativeBucketIamPolicy(bucket_name);

    if (!current_policy) {
      throw std::runtime_error(current_policy.status().message());
    }

    for (auto& binding : current_policy->bindings()) {
      if (binding.role() != "roles/storage.objectViewer") {
        continue;
      }
      auto& members = binding.members();
      if (std::find(members.begin(), members.end(), "allUsers") ==
          members.end()) {
        members.emplace_back("allUsers");
      }
    }

    auto updated_policy =
        client.SetNativeBucketIamPolicy(bucket_name, *current_policy);

    if (!updated_policy) {
      throw std::runtime_error(current_policy.status().message());
    }
  }
  // [END native storage_set_bucket_public_iam]
  (std::move(client), bucket_name);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << "\n";
    return 1;
  }
  //! [create client]

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"get-bucket-iam-policy", &GetBucketIamPolicy},
      {"native-get-bucket-iam-policy", &NativeGetBucketIamPolicy},
      {"add-bucket-iam-member", &AddBucketIamMember},
      {"native-add-bucket-iam-member", &NativeAddBucketIamMember},
      {"remove-bucket-iam-member", &RemoveBucketIamMember},
      {"native-remove-bucket-iam-member", &NativeRemoveBucketIamMember},
      {"test-bucket-iam-permissions", &TestBucketIamPermissions},
      {"set-bucket-public-iam", &SetBucketPublicIam},
      {"native-set-bucket-public-iam", &NativeSetBucketPublicIam},
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
