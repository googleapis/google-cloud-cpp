# Cloud Optimization API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Optimization API][cloud-service-root], a service that provides a
portfolio of solvers to address common optimization use cases.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/optimization/v1/fleet_routing_client.h"
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
  namespace optimization = ::google::cloud::optimization_v1;

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
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Optimization API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/optimization/docs
[cloud-service-root]: https://cloud.google.com/optimization
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/optimization/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/optimization
