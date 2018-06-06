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

#include "google/cloud/storage/bucket.h"
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

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Examples:\n";
  for (auto example : {"get-metadata <bucket_name>"}) {
    std::cerr << "  " << program << " " << example << "\n";
  }
  std::cerr << std::flush;
}

//! [get metadata]
void GetMetadata(storage::Bucket bucket, int& argc, char* argv[]) {
  auto meta = bucket.GetMetadata();
  std::cout << "The metadata is " << meta << std::endl;
}
//! [get metadata]

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(storage::Bucket, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"get-metadata", &GetMetadata},
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
  if (argc < 2) {
    PrintUsage(argc, argv, "Missing bucket-name");
    return 1;
  }
  std::string const bucket_name = ConsumeArg(argc, argv);

  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  auto client =
      storage::CreateDefaultClient(storage::GoogleDefaultCredentials());
  //! [create client]

  // Create the object to manage a bucket:
  storage::Bucket bucket(client, bucket_name);

  it->second(bucket, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
