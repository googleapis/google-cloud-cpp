// Copyright 2018 Google Inc.
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

#include "echo.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <future>
#include <iostream>

class EchoImpl : public Echo::Service {
 public:
  EchoImpl() {}

  grpc::Status Ping(grpc::ServerContext* context, Request const* request,
                    Response* response) override {
    response->set_value(request->value());
    return grpc::Status::OK;
  }

  virtual grpc::Status StreamPing(
      grpc::ServerContext* context, Request const* request,
      grpc::ServerWriter<Response>* writer) override {
    Response response;
    writer->WriteLast(response, grpc::WriteOptions());
    return grpc::Status::OK;
  }
};

struct Replica {
  std::string address;
  std::shared_ptr<grpc::Server> server;
  std::future<void> task;
};

Replica CreateReplica(EchoImpl* echo_impl, std::string address) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(echo_impl);
  std::shared_ptr<grpc::Server> server = builder.BuildAndStart();

  auto waiter = [](std::shared_ptr<grpc::Server> server) { server->Wait(); };
  auto task = std::async(std::launch::async, waiter, server);
  return Replica{std::move(address), std::move(server), std::move(task)};
}

int main(int argc, char* argv[]) try {
  if (argc < 3) {
    std::cerr << "Usage: server <port> [port ...]" << std::endl;
    return 1;
  }

  EchoImpl echo_impl;
  std::vector<Replica> servers;
  // Create a server for each port and launch a thread to run it.
  for (int i = 1; i != argc; ++i) {
    std::string address = "0.0.0.0:" + std::string(argv[i]);
    servers.emplace_back(CreateReplica(&echo_impl, std::move(address)));
  }

  // Continuously restart each server, to force reconnects from the client.
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(20));
    for (auto& replica : servers) {
      replica.server->Shutdown();
      replica.task.get();
    }
    std::cout << "Shutdown completed." << std::endl;
    for (auto& replica : servers) {
      replica = CreateReplica(&echo_impl, std::move(replica.address));
    }
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
