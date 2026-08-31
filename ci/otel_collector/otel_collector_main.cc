// Copyright 2026 Google LLC
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

#include "ci/otel_collector/otel_collector.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  std::string server_address = "0.0.0.0:50051";
  if (argc > 1) {
    server_address = "0.0.0.0:" + std::string(argv[1]);
  }

  google::cloud::testing_util::OtelCollectorServer service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(
      static_cast<google::monitoring::v3::MetricService::Service*>(&service));
  builder.RegisterService(
      static_cast<google::cloud::opentelemetry::testing::
                      ObservabilityVerificationService::Service*>(&service));

  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  if (!server) {
    std::cerr << "Failed to start OtelCollectorServer on " << server_address
              << std::endl;
    return 1;
  }

  std::cout << "Observability Collector Server listening on " << server_address
            << std::endl;
  server->Wait();
  return 0;
}
