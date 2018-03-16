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

void MakeStreamPing(Echo::Stub& echo, std::int32_t count) {
  for (int i = 0; i != 100; ++i) {
    grpc::ClientContext context;
    Request request;
    request.set_value(count);
    auto stream = echo.StreamPing(&context, request);

    Response response;
    while (stream->Read(&response)) {
    }
    auto status = stream->Finish();
    if (status.ok()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::cerr << "Error making StreamPing request" << std::endl;
}

void MakePing(Echo::Stub& echo, std::int32_t count) {
  for (int i = 0; i != 100; ++i) {
    grpc::ClientContext context;
    Request request;
    request.set_value(count);
    Response response;
    auto status = echo.Ping(&context, request, &response);
    if (status.ok()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::cerr << "Error making Ping request" << std::endl;
}

void RunClient(std::string const& server_address, std::chrono::minutes duration,
               int id) {
  grpc::ChannelArguments arguments;
  arguments.SetInt("foo-bar-baz", id);

  auto channel = grpc::CreateCustomChannel(
      server_address, grpc::InsecureChannelCredentials(), arguments);
  auto echo = Echo::NewStub(channel);

  auto deadline = std::chrono::system_clock::now() + duration;
  std::int32_t count = 0;
  while (std::chrono::system_clock::now() < deadline) {
    MakeStreamPing(*echo, count);
    MakeStreamPing(*echo, count);
    MakePing(*echo, count);
    if (++count % 100000 == 0) {
      std::cout << "." << std::flush;
    }
  }
}

int main(int argc, char* argv[]) try {
  if (argc < 4) {
    std::cerr << "Usage: client <address> <thread-count>"
              << " <test-duration-in-minutes>" << std::endl;
    return 1;
  }

  std::string const server_address = argv[1];
  int const thread_count = std::stoi(argv[2]);
  auto test_duration = std::chrono::minutes(std::stol(argv[3]));

  std::cout << "Running client threads: " << std::flush;
  std::vector<std::future<void>> tasks;
  for (int i = 0; i != thread_count; ++i) {
    tasks.emplace_back(std::async(std::launch::async, RunClient, server_address,
                                  test_duration, i));
  }

  for (auto& t : tasks) {
    t.get();
  }
  std::cout << " DONE." << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
