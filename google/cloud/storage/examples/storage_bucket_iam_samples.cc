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

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << std::endl;
}

void GetBucketIamPolicy(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-iam-policy <bucket_name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket iam policy] [START storage_view_bucket_iam_members]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<google::cloud::IamPolicy> policy =
        client.GetBucketIamPolicy(bucket_name);

    if (!policy) {
      std::cerr << "Error getting IAM policy for bucket " << bucket_name
                << ", status=" << policy.status() << std::endl;
      return;
    }

    std::cout << "The IAM policy for bucket " << bucket_name << " is "
              << *policy << std::endl;
  }
  //! [get bucket iam policy] [END storage_view_bucket_iam_members]
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
  //! [add bucket iam member] [START storage_add_bucket_iam_member]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string role,
     std::string member) {
    StatusOr<google::cloud::IamPolicy> policy =
        client.GetBucketIamPolicy(bucket_name);

    if (!policy) {
      std::cerr << "Error getting current IAM policy for bucket " << bucket_name
                << ", status=" << policy.status() << std::endl;
      return;
    }

    policy->bindings.AddMember(role, member);

    StatusOr<google::cloud::IamPolicy> updated_policy =
        client.SetBucketIamPolicy(bucket_name, *policy);

    if (!updated_policy) {
      std::cerr << "Error setting IAM policy for bucket " << bucket_name
                << ", status=" << updated_policy.status() << std::endl;
      return;
    }

    std::cout << "Updated IAM policy bucket " << bucket_name
              << ". The new policy is " << *updated_policy << std::endl;
  }
  //! [add bucket iam member] [END storage_add_bucket_iam_member]
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
  //! [remove bucket iam member] [START storage_remove_bucket_iam_member]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string role,
     std::string member) {
    StatusOr<google::cloud::IamPolicy> policy =
        client.GetBucketIamPolicy(bucket_name);
    if (!policy) {
      std::cerr << "Error getting current IAM policy for bucket " << bucket_name
                << ", status=" << policy.status() << std::endl;
      return;
    }

    policy->bindings.RemoveMember(role, member);

    StatusOr<google::cloud::IamPolicy> updated_policy =
        client.SetBucketIamPolicy(bucket_name, *policy);

    if (!updated_policy) {
      std::cerr << "Error setting IAM policy for bucket " << bucket_name
                << ", status=" << updated_policy.status() << std::endl;
      return;
    }

    std::cout << "Updated IAM policy bucket " << bucket_name
              << ". The new policy is " << *updated_policy << std::endl;
  }
  //! [remove bucket iam member] [END storage_remove_bucket_iam_member]
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name,
     std::vector<std::string> permissions) {
    StatusOr<std::vector<std::string>> actual_permissions =
        client.TestBucketIamPermissions(bucket_name, permissions);

    if (!actual_permissions) {
      std::cerr << "Error checking IAM permissions for bucket " << bucket_name
                << ", status=" << actual_permissions.status() << std::endl;
      return;
    }

    if (actual_permissions->empty()) {
      std::cout << "The caller does not hold any of the tested permissions the"
                << " bucket " << bucket_name << std::endl;
      return;
    }

    std::cout << "The caller is authorized for the following permissions on "
              << bucket_name << ": ";
    for (auto const& permission : *actual_permissions) {
      std::cout << "\n    " << permission;
    }
    std::cout << std::endl;
  }
  //! [test bucket iam permissions]
  (std::move(client), bucket_name, permissions);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << std::endl;
    return 1;
  }
  //! [create client]

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"get-bucket-iam-policy", &GetBucketIamPolicy},
      {"add-bucket-iam-member", &AddBucketIamMember},
      {"remove-bucket-iam-member", &RemoveBucketIamMember},
      {"test-bucket-iam-permissions", &TestBucketIamPermissions},
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
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
