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

#include "google/cloud/optimization/fleet_routing_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id destination\n"
              << "  destination is a GCS bucket\n";
    return 1;
  }
  // This quickstart loads an example model from a known GCS bucket. The service
  // solves the model, and writes the solution to the destination GCS bucket.
  std::string const source =
      "gs://cloud-samples-data/optimization-ai/async_request_model.json";
  auto const destination =
      std::string{"gs://"} + argv[2] + "/optimization_quickstart_solution.json";

  namespace gc = ::google::cloud;
  namespace optimization = ::google::cloud::optimization;

  auto options = gc::Options{}.set<gc::UserProjectOption>(argv[1]);
  auto client = optimization::FleetRoutingClient(
      optimization::MakeFleetRoutingConnection(options));

  google::cloud::optimization::v1::BatchOptimizeToursRequest req;
  req.set_parent(gc::Project(argv[1]).FullName());
  auto& config = *req.add_model_configs();
  auto& input = *config.mutable_input_config();
  input.mutable_gcs_source()->set_uri(source);
  input.set_data_format(google::cloud::optimization::v1::JSON);
  auto& output = *config.mutable_output_config();
  output.mutable_gcs_destination()->set_uri(destination);
  output.set_data_format(google::cloud::optimization::v1::JSON);

  auto fut = client.BatchOptimizeTours(req);
  std::cout << "Request sent to the service...\n";
  auto resp = fut.get();
  if (!resp) throw std::move(resp).status();
  std::cout << "Solution written to: " << destination << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
