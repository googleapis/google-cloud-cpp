// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
//! [grpc-includes] [START storage_grpc_quickstart]
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/options.h"

//! [grpc-includes] [END storage_grpc_quickstart]

namespace {
namespace examples = ::google::cloud::storage::examples;

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
//! [grpc-read-write] [START storage_grpc_quickstart]
void GrpcReadWrite(std::string const& bucket_name) {
  namespace gcs = ::google::cloud::storage;
  auto constexpr kText = R"""(Hello World!)""";

  //! [grpc-default-client]
  auto client = google::cloud::storage_experimental::DefaultGrpcClient();
  //! [grpc-default-client]

  auto object = client.InsertObject(bucket_name, "lorem.txt", kText);
  if (!object) throw std::move(object).status();

  auto input = client.ReadObject(bucket_name, "lorem.txt",
                                 gcs::Generation(object->generation()));
  std::string const actual(std::istreambuf_iterator<char>{input}, {});
  std::cout << "The contents read back are:\n"
            << actual
            << "\nThe received checksums are: " << input.received_hash()
            << "\nThe computed checksums are: " << input.computed_hash()
            << "\nThe original hashes    are: crc32c=" << object->crc32c()
            << ",md5=" << object->md5_hash() << "\n";
}
//! [grpc-read-write] [END storage_grpc_quickstart]

//! [grpc-with-dp] [START storage_grpc_quickstart_dp]
void GrpcClientWithDP() {
  namespace g = ::google::cloud;

  auto client = google::cloud::storage_experimental::DefaultGrpcClient(
      g::Options{}.set<g::EndpointOption>(
          "google-c2p:///storage.googleapis.com"));
  // Use `client` as usual.
}
//! [grpc-with-dp] [END storage_grpc_quickstart_dp]

//! [grpc-client-with-project]
void GrpcClientWithProject(std::string project_id) {
  namespace gcs = ::google::cloud::storage;
  auto client = google::cloud::storage_experimental::DefaultGrpcClient(
      google::cloud::Options{}.set<gcs::ProjectIdOption>(
          std::move(project_id)));
  std::cout << "Successfully created a gcs::Client configured to use gRPC\n";
}
//! [grpc-client-with-project]

#else

void GrpcReadWrite(std::string const&) {}
void GrpcClientWithDP() {}
void GrpcClientWithProject(std::string const&) {}

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

void GrpcReadWriteCommand(std::vector<std::string> argv) {
  if (argv.size() != 1 || argv[0] == "--help") {
    throw examples::Usage("grpc-read-write <bucket-name>");
  }
  GrpcReadWrite(argv[0]);
}

void GrpcClientWithDPCommand(std::vector<std::string> const& argv) {
  if (argv.empty()) return GrpcClientWithDP();
  throw examples::Usage("grpc-client-with-dp");
}

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

  // The DP example requires running on a GCE instance with DP enabled.
  std::cout << "Running GrpcReadWriteDP() example" << std::endl;
  GrpcClientWithDPCommand({});

  std::cout << "Running GrpcClientWithProjectCommand() example" << std::endl;
  GrpcClientWithProjectCommand({project_id});
}

}  // namespace

int main(int argc, char* argv[]) {
  examples::Example example({
      {"grpc-read-write", GrpcReadWriteCommand},
      {"grpc-client-with-dp", GrpcClientWithDPCommand},
      {"grpc-client-with-project", GrpcClientWithProjectCommand},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
