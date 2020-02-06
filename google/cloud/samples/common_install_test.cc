// Copyright 2019 Google LLC
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

#include "google/cloud/grpc_utils/completion_queue.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/grpc_utils/version.h"
#include "google/cloud/version.h"
#include <iostream>

int main() {
  std::cout
      << "Verify symbols from google_cloud_cpp_common are usable. version="
      << google::cloud::version_string() << "\n";
  std::cout
      << "Verify symbols from google_cloud_cpp_grpc_utils are usable. version="
      << google::cloud::grpc_utils::version_string() << "\n";

  google::cloud::grpc_utils::CompletionQueue cq;
  std::thread t{[&cq] { cq.Run(); }};
  std::cout
      << "Verify symbols from google_cloud_cpp_grpc_utils are usable. version="
      << google::cloud::grpc_utils::MakeStatusFromRpcError(
             grpc::Status(grpc::StatusCode::UNKNOWN, "Just for testing"))
      << "\n";
  cq.Shutdown();
  t.join();

  return 0;
}
