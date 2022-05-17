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
#include <google/protobuf/text_format.h>
#include <iostream>
#include <stdexcept>

google::cloud::optimization::v1::OptimizeToursRequest LoadExampleModel();

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace gc = ::google::cloud;
  namespace optimization = ::google::cloud::optimization;

  auto options = gc::Options{}.set<gc::UserProjectOption>(argv[1]);
  auto client = optimization::FleetRoutingClient(
      optimization::MakeFleetRoutingConnection(options));

  auto req = LoadExampleModel();
  req.set_parent(gc::Project(argv[1]).FullName());

  auto resp = client.OptimizeTours(req);
  if (!resp) throw std::runtime_error(resp.status().message());
  std::cout << resp->DebugString() << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}

/**
 * Returns a simple example request with exactly one vehicle and one shipment.
 *
 * It is not an interesting use case of the service, but it shows how to
 * interact with the proto fields.
 *
 * For a more interesting example that optimizes multiple vehicles picking up
 * multiple shipments, see the proto listed at:
 * https://cloud.google.com/optimization/docs/introduction/example_problem#complete_request
 *
 * Protos can also be loaded from strings as follows:
 *
 * @code
 * #include <google/protobuf/text_format.h>
 *
 * google::cloud::optimization::v1::OptimizeToursRequest req;
 * google::protobuf::TextFormat::ParseFromString("model { ... }", &req);
 * @endcode
 */
google::cloud::optimization::v1::OptimizeToursRequest LoadExampleModel() {
  google::cloud::optimization::v1::OptimizeToursRequest req;

  // Set the model's time constraints
  req.mutable_model()->mutable_global_start_time()->set_seconds(0);
  req.mutable_model()->mutable_global_end_time()->set_seconds(1792800);

  // Add one vehicle and its constraints
  auto& vehicle = *req.mutable_model()->add_vehicles();
  auto& start_location = *vehicle.mutable_start_location();
  start_location.set_latitude(48.863102);
  start_location.set_longitude(2.341204);
  vehicle.set_cost_per_traveled_hour(3600);
  vehicle.add_end_time_windows()->mutable_end_time()->set_seconds(1236000);
  (*vehicle.mutable_load_limits())["weight"].set_max_load(800);

  // Add one shipment and its constraints
  auto& shipment = *req.mutable_model()->add_shipments();
  auto& pickup = *shipment.add_pickups();
  auto& arrival_location = *pickup.mutable_arrival_location();
  arrival_location.set_latitude(48.843561);
  arrival_location.set_longitude(2.297602);
  pickup.mutable_duration()->set_seconds(90000);
  auto& window = *pickup.add_time_windows();
  window.mutable_start_time()->set_seconds(912000);
  window.mutable_end_time()->set_seconds(967000);
  (*shipment.mutable_load_demands())["weight"].set_amount(40);

  return req;
}
