// Copyright 2020 Google LLC
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

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/getenv.h"

namespace {

//! [grpc-read-write]
void GrpcReadWrite(std::string const& bucket_name) {
  namespace gcs = google::cloud::storage;
  auto const text = R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
)""";

  //! [grpc-default-client]
  auto client = google::cloud::storage_experimental::DefaultGrpcClient();

  auto object = client->InsertObject(bucket_name, "lorem.txt", text);
  //! [grpc-default-client]
  if (!object) throw std::runtime_error(object.status().message());

  auto input = client->ReadObject(bucket_name, "lorem.txt",
                                  gcs::Generation(object->generation()));
  std::string const actual(std::istreambuf_iterator<char>{input}, {});
  std::cout << "The contents read back are:\n"
            << actual
            << "\nThe received checksums are: " << input.received_hash()
            << "\nThe computed checksums are: " << input.computed_hash()
            << "\nThe original hashes    are: crc32c=" << object->crc32c()
            << ",md5=" << object->md5_hash() << "\n";
}
//! [grpc-read-write]

void GrpcReadWriteCommand(std::vector<std::string> argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("grpc-read-write <bucket-name>");
  }
  GrpcReadWrite(argv[0]);
}

//! [grpc-client-with-project]
void GrpcClientWithProject(std::string project_id) {
  namespace gcs = google::cloud::storage;
  auto options = gcs::ClientOptions::CreateDefaultClientOptions();
  if (!options) throw std::runtime_error(options.status().message());
  auto client = google::cloud::storage_experimental::DefaultGrpcClient(
      options->set_project_id(std::move(project_id)));
  std::cout << "Successfully created a gcs::Client configured to use gRPC\n";
}
//! [grpc-client-with-project]

void GrpcClientWithProjectCommand(std::vector<std::string> argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("grpc-client-with-project <project-id>");
  }
  GrpcClientWithProject(argv[0]);
}

void RunAll(std::vector<std::string> const&);

int RunOneCommand(std::vector<std::string> argv) {
  using CommandType = std::function<void(std::vector<std::string> const&)>;
  using CommandMap = std::map<std::string, CommandType>;

  CommandMap commands = {
      {"auto", RunAll},
      {"grpc-read-write", GrpcReadWriteCommand},
      {"grpc-client-with-project", GrpcClientWithProjectCommand},
  };

  static std::string const kUsageMsg = [&argv, &commands] {
    auto last_slash = argv[0].find_last_of("/\\");
    auto program = argv[0].substr(last_slash + 1);
    std::string usage;
    usage += "Usage: ";
    usage += program;
    usage += " <command> [arguments]\n\n";
    usage += "Commands:\n";
    for (auto const& kv : commands) {
      if (kv.first == "auto") continue;
      try {
        kv.second({});
      } catch (std::exception const& ex) {
        usage += "    ";
        usage += ex.what();
        usage += "\n";
      }
    }
    return usage;
  }();

  if (argv.size() < 2) {
    std::cerr << "Missing command argument\n" << kUsageMsg << "\n";
    return 1;
  }
  std::string command_name = argv[1];
  argv.erase(argv.begin());  // remove the program name from the list.
  argv.erase(argv.begin());  // remove the command name from the list.

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    std::cerr << "Unknown command " << command_name << "\n"
              << kUsageMsg << "\n";
    return 1;
  }

  // Run the command.
  command->second(argv);
  return 0;
}

void RunAll(std::vector<std::string> const&) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }
  auto bucket_name =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BUCKET_NAME")
          .value_or("");
  if (bucket_name.empty()) {
    throw std::runtime_error(
        "GOOGLE_CLOUD_CPP_BUCKET_NAME is not set or is empty");
  }

  RunOneCommand({"", "grpc-read-write", bucket_name});
  RunOneCommand({"", "grpc-client-with-project", project_id});
}

bool AutoRun() {
  return google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
             .value_or("") == "yes";
}

}  // namespace

int main(int ac, char* av[]) try {
  if (AutoRun()) {
    RunAll({});
    return 0;
  }

  return RunOneCommand({av, av + ac});
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  return 1;
}
