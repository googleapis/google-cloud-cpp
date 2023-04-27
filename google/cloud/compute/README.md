# Compute Engine API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Compute Engine API][cloud-service-docs], a service to create challenges
and verify attestation responses.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/compute/instances/v1/instances_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }

  namespace instances = ::google::cloud::compute_instances_v1;
  auto client = instances::InstancesClient(
      google::cloud::ExperimentalTag{},
      instances::MakeInstancesConnectionRest(google::cloud::ExperimentalTag{}));

  google::cloud::cpp::compute::instances::v1::ListInstancesRequest request;
  request.set_project(argv[1]);
  request.set_zone(argv[2]);
  request.set_max_results(1);
  for (auto instance : client.ListInstances(std::move(request))) {
    if (!instance) throw std::move(instance).status();
    std::cout << instance->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Compute Engine API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/compute
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-compute/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/compute
