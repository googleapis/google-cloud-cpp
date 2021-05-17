// Copyright 2021 Google LLC
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

#include <grpcpp/grpcpp.h>
#include <hello_world.grpc.pb.h>
#include <iostream>

class GreeterImpl final : public google::cloud::examples::Greet::Service {
 public:
  grpc::Status Hello(
      grpc::ServerContext*, google::cloud::examples::HelloRequest const*,
      google::cloud::examples::HelloResponse* response) override {
    response->set_greeting("Hello World");
    return grpc::Status::OK;
  }
};

int main() {
  char const* port = std::getenv("PORT");
  if (port == nullptr) {
    std::cout
        << R"""({"severity": "info", "message": "defaulting PORT to 8080"}\n)""";
    port = "8080";
  }
  auto const server_address = std::string{"0.0.0.0:"} + port;
  GreeterImpl impl;
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&impl);
  auto server = builder.BuildAndStart();
  server->Wait();

  return 0;
}
