# Cloud Optimization API C++ Client Library

:construction:

This directory contains an idiomatic C++ client library for the
[Cloud Optimization API][cloud-service-root], a service that provides a
portfolio of solvers to address common optimization use cases.

This library is **experimental**. Its APIs are subject to change without notice.

Please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++11 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Cloud Optimization API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service-root]: https://cloud.google.com/optimization
[cloud-service-docs]: https://cloud.google.com/optimization/docs
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-optimization/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/optimization

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
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
```
<!-- inject-quickstart-end -->

* Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should read the current document.
* Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.
