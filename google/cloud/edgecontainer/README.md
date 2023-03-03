# Distributed Cloud Edge Container API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Distributed Cloud Edge Container API][cloud-service-docs], a service that
allows you to run Kubernetes clusters on dedicated hardware provided and
maintained by Google that is separate from the Google Cloud data center.

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
#include "google/cloud/edgecontainer/v1/edge_container_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace edgecontainer = ::google::cloud::edgecontainer_v1;
  auto client = edgecontainer::EdgeContainerClient(
      edgecontainer::MakeEdgeContainerConnection());

  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + argv[2];
  for (auto r : client.ListClusters(parent)) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Distributed Cloud Edge Container API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/distributed-cloud/edge/latest/docs
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-edgecontainer/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/edgecontainer
