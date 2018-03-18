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

int main(int argc, char* argv[]) try {
  if (argc < 3) {
    std::cerr << "Usage: server <port> <thread-count>" << std::endl;
    return 1;
  }

  std::string const address = "localhost:" + std::string(argv[1]);
  int const thread_count = std::stoi(argv[2]);

  EchoImpl echo_impl;

  while (true) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&echo_impl);
    auto server = builder.BuildAndStart();

    std::vector<std::future<void>> tasks;
    for (int i = 0; i != thread_count; ++i) {
      tasks.emplace_back(
          std::async(std::launch::async, [&server]() { server->Wait(); }));
    }
    std::this_thread::sleep_for(std::chrono::seconds(20));
    server->Shutdown();
    for (auto& t : tasks) {
      t.get();
    }
    std::cout << "Shutdown completed." << std::endl;
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
