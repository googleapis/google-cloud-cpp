// Copyright 2023 Google LLC
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
//
// Runs the CloudBigtableTestProxy as a server.

#include "google/cloud/bigtable/test_proxy/cbt_test_proxy.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/log.h"
#include <grpcpp/server_builder.h>
#include <string>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>\n";
    return 1;
  }

  std::string server_address = absl::StrCat("[::]:", argv[1]);

  ::grpc::ServerBuilder builder;
  auto creds = grpc::experimental::LocalServerCredentials(LOCAL_TCP);
  builder.AddListeningPort(server_address, creds);

  google::cloud::bigtable::test_proxy::CbtTestProxy proxy;
  builder.RegisterService(&proxy);

  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  GCP_LOG(INFO) << "Server listening on " << server_address;

  server->Wait();
  return 0;
}
