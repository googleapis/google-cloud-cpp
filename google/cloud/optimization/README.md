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
* C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
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
#include <iostream>
#include <stdexcept>

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
  if (!resp) throw std::runtime_error(resp.status().message());
  std::cout << "Solution written to: " << destination << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
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
