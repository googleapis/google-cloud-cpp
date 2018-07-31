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

namespace storage = google::cloud::storage;

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

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argc > 1 ? argv[0] : "unknown";
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Examples:\n";
  for (auto const& example : {"get-bucket-metadata <bucket-name>"}) {
    std::cerr << "  " << program << " " << example << "\n";
  }
  std::cerr << std::flush;
}

void ListBuckets(storage::Client client, int& argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-buckets"};
  }
  //! [list buckets] [START storage_list_buckets]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client) {
    int count = 0;
    for (gcs::BucketMetadata const& meta : client.ListBuckets()) {
      std::cout << meta.name() << std::endl;
      ++count;
    }
    if (count == 0) {
      std::cout << "No buckets in default project" << std::endl;
    }
  }
  //! [list buckets] [END storage_list_buckets]
  (std::move(client));
}

void ListBucketsForProject(storage::Client client, int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-buckets-for-project <project-id>"};
  }
  auto project_id = ConsumeArg(argc, argv);
  //! [list buckets for project]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string project_id) {
    int count = 0;
    for (gcs::BucketMetadata const& meta :
         client.ListBucketsForProject(project_id)) {
      std::cout << meta.name() << std::endl;
      ++count;
    }
    if (count == 0) {
      std::cout << "No buckets in project " << project_id << std::endl;
    }
  }
  //! [list buckets for project]
  (std::move(client), project_id);
}

//! [get bucket metadata]
void GetBucketMetadata(storage::Client client, int& argc, char* argv[]) {
  if (argc < 2) {
    throw Usage{"get-bucket-metadata <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto meta = client.GetBucketMetadata(bucket_name);
  std::cout << "The metadata is " << meta << std::endl;
}
//! [get bucket metadata]

//! [list objects]
void ListObjects(storage::Client client, int& argc, char* argv[]) {
  if (argc < 2) {
    throw Usage{"list-objects <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  for (auto const& meta : client.ListObjects(bucket_name)) {
    std::cout << "bucket_name=" << meta.bucket()
              << ", object_name=" << meta.name() << std::endl;
  }
}
//! [list objects]

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"list-buckets", &ListBuckets},
      {"list-buckets-for-project", &ListBucketsForProject},
      {"get-bucket-metadata", &GetBucketMetadata},
      {"list-objects", &ListObjects},
  };

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

  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  storage::Client client;
  //! [create client]

  it->second(client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
