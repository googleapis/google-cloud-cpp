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

void ListObjects(google::cloud::storage::Client client, int& argc,
                 char* argv[]) {
  if (argc < 2) {
    throw Usage{"list-objects <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [list objects] [START storage_list_files]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name) {
    for (gcs::ObjectMetadata const& meta : client.ListObjects(bucket_name)) {
      std::cout << "bucket_name=" << meta.bucket()
                << ", object_name=" << meta.name() << std::endl;
    }
  }
  //! [list objects] [END storage_list_files]
  (std::move(client), bucket_name);
}

void InsertObject(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc < 3) {
    throw Usage{
        "insert-object <bucket-name> <object-name> <object-contents (string)>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto contents = ConsumeArg(argc, argv);
  //! [insert object] [START storage_upload_file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string contents) {
    gcs::ObjectMetadata meta =
        client.InsertObject(bucket_name, object_name, std::move(contents));
    std::cout << "The file was uploaded. The new object metadata is " << meta
              << std::endl;
  }
  //! [insert object] [END storage_upload_file]
  (std::move(client), bucket_name, object_name, contents);
}

void GetObjectMetadata(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc < 3) {
    throw Usage{"get-object-metadata <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [get object metadata] [START storage_get_metadata]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    gcs::ObjectMetadata meta =
        client.GetObjectMetadata(bucket_name, object_name);
    std::cout << "The metadata is " << meta << std::endl;
  }
  //! [get object metadata] [END storage_get_metadata]
  (std::move(client), bucket_name, object_name);
}

void ReadObject(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc < 2) {
    throw Usage{"read-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [read object]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);
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

void DeleteObject(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc < 2) {
    throw Usage{"delete-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [delete object] [START storage_delete_file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    client.DeleteObject(bucket_name, object_name);
    std::cout << "Deleted " << object_name << " in bucket " << bucket_name
              << std::endl;
  }
  //! [delete object] [END storage_delete_file]
  (std::move(client), bucket_name, object_name);
}

void WriteObject(google::cloud::storage::Client client, int& argc,
                 char* argv[]) {
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
    gcs::ObjectWriteStream stream =
        client.WriteObject(bucket_name, object_name);

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

void UpdateObjectMetadata(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "update-object-metadata <bucket-name> <object-name> <key> <value>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto key = ConsumeArg(argc, argv);
  auto value = ConsumeArg(argc, argv);
  //! [update object metadata] [START storage_set_metadata]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string key, std::string value) {
    gcs::ObjectMetadata meta =
        client.GetObjectMetadata(bucket_name, object_name);
    gcs::ObjectMetadata desired = meta;
    desired.mutable_metadata().emplace(key, value);
    gcs::ObjectMetadata updated = client.UpdateObject(
        bucket_name, object_name, desired, gcs::IfMatchEtag(meta.etag()));
    std::cout << "Object updated. The full metadata after the update is: "
              << updated << std::endl;
  }
  //! [update object metadata] [END storage_set_metadata]
  (std::move(client), bucket_name, object_name, key, value);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  google::cloud::storage::Client client;
  //! [create client]

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"list-objects", &ListObjects},
      {"insert-object", &InsertObject},
      {"get-object-metadata", &GetObjectMetadata},
      {"read-object", &ReadObject},
      {"delete-object", &DeleteObject},
      {"write-object", &WriteObject},
      {"update-object-metadata", &UpdateObjectMetadata},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 1;
      kv.second(client, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
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

  it->second(client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
