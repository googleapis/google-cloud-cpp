// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/servicecontrol/service_controller_client.h"
#include "google/cloud/project.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace servicecontrol = ::google::cloud::servicecontrol;
  auto client = servicecontrol::ServiceControllerClient(
      servicecontrol::MakeServiceControllerConnection());

  auto const project = google::cloud::Project(argv[1]);
  google::api::servicecontrol::v1::CheckRequest request;
  request.set_service_name("pubsub.googleapis.com");
  *request.mutable_operation() = [&] {
    using ::google::protobuf::util::TimeUtil;

    google::api::servicecontrol::v1::Operation op;
    op.set_operation_id("TODO-use-UUID-4-or-UUID-5");
    op.set_operation_name("google.pubsub.v1.Publisher.Publish");
    op.set_consumer_id(project.FullName());
    *op.mutable_start_time() = TimeUtil::GetCurrentTime();
    return op;
  }();

  auto response = client.Check(request);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
