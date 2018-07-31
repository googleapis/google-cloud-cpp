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

namespace gcs = google::cloud::storage;

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
  for (auto const& example :
       {"read-object <bucket-name> <object-name>",
        "insert-object <object name> <object contents>"}) {
    std::cerr << "  " << program << " " << example << "\n";
  }
  std::cerr << std::flush;
}

//! [insert object]
void InsertObject(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 3) {
    throw Usage{
        "insert-object <bucket-name> <object-name> <object-contents (string)>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto contents = ConsumeArg(argc, argv);
  auto meta =
      client.InsertObject(bucket_name, object_name, std::move(contents));
  std::cout << "The new object metadata is " << meta << std::endl;
}
//! [insert object]

void GetObjectMetadata(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 3) {
    throw Usage{"get-object-metadata <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [get object metadata] [START storage_get_metadata]
  [](google::cloud::storage::Client client, std::string bucket_name,
     std::string object_name) {
    auto meta = client.GetObjectMetadata(bucket_name, object_name);
    std::cout << "The metadata is " << meta << std::endl;
  }
  //! [get object metadata] [END storage_get_metadata]
  (std::move(client), bucket_name, object_name);
}

void ReadObject(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 2) {
    throw Usage{"read-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [read object]
  [](google::cloud::storage::Client client, std::string bucket_name,
     std::string object_name) {
    auto stream = client.ReadObject(bucket_name, object_name);
    int count = 0;
    while (not stream.eof()) {
      std::string line;
      std::getline(stream, line, '\n');
      ++count;
    }
    std::cout << "The object has " << count << " lines" << std::endl;
  }
  //! [read object]
  (std::move(client), bucket_name, object_name);
}

//! [delete object]
void DeleteObject(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 2) {
    throw Usage{"delete-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  client.DeleteObject(bucket_name, object_name);
  std::cout << "Deleted " << object_name << " in bucket " << bucket_name
            << std::endl;
}
//! [delete object]

void WriteObject(gcs::Client client, int& argc, char* argv[]) {
  if (argc < 3) {
    throw Usage{
        "write-object <bucket-name> <object-name> <target-object-line-count>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto desired_line_count = std::stol(ConsumeArg(argc, argv));

  //! [write object]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     long desired_line_count) {
    std::string const text = "Lorem ipsum dolor sit amet";
    auto stream = client.WriteObject(bucket_name, object_name);

    for (int lineno = 0; lineno != desired_line_count; ++lineno) {
      // Add 1 to the counter, because it is conventional to number lines
      // starting at 1.
      stream << (lineno + 1) << ": " << text << "\n";
    }

    gcs::ObjectMetadata meta = stream.Close();
    std::cout << "The resulting object size is: " << meta.size() << std::endl;
  }
  //! [write object]
  (std::move(client), bucket_name, object_name, desired_line_count);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(gcs::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"read-object", &ReadObject},
      {"insert-object", &InsertObject},
      {"get-object-metadata", &GetObjectMetadata},
      {"delete-object", &DeleteObject},
      {"write-object", &WriteObject},
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
  gcs::Client client;
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
