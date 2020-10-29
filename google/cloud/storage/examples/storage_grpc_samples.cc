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

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/internal/getenv.h"

namespace {
namespace examples = ::google::cloud::storage::examples;

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
//! [grpc-read-write]
void GrpcReadWrite(std::string const& bucket_name) {
  namespace gcs = google::cloud::storage;
  auto constexpr kText = R"""(Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
)""";

  //! [grpc-default-client]
  auto client = google::cloud::storage_experimental::DefaultGrpcClient();
  //! [grpc-default-client]

  auto object = client->InsertObject(bucket_name, "lorem.txt", kText);
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
#else
void GrpcReadWrite(std::string const&) {}
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

void GrpcReadWriteCommand(std::vector<std::string> argv) {
  if (argv.size() != 1 || argv[0] == "--help") {
    throw examples::Usage("grpc-read-write <bucket-name>");
  }
  GrpcReadWrite(argv[0]);
}

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
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
#else
void GrpcClientWithProject(std::string const&) {}
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

void GrpcClientWithProjectCommand(std::vector<std::string> argv) {
  if (argv.size() != 1 || argv[0] == "--help") {
    throw examples::Usage("grpc-client-with-project <project-id>");
  }
  GrpcClientWithProject(argv[0]);
}

void AutoRun(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto bucket_name = google::cloud::internal::GetEnv(
                         "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                         .value();

  std::cout << "Running GrpcReadWrite() example" << std::endl;
  GrpcReadWriteCommand({bucket_name});

  std::cout << "Running GrpcReadWrite() example" << std::endl;
  GrpcClientWithProjectCommand({project_id});
}

}  // namespace

int main(int argc, char* argv[]) {
  examples::Example example({
      {"grpc-read-write", GrpcReadWriteCommand},
      {"grpc-client-with-project", GrpcClientWithProjectCommand},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
