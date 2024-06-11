# Network Services API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Network Services API][cloud-service-docs]. This is part of Traffic Director:
Google Cloud's fully managed application networking platform and service mesh.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/networkservices/v1/network_services_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "global");

  namespace networkservices = ::google::cloud::networkservices_v1;
  auto client = networkservices::NetworkServicesClient(
      networkservices::MakeNetworkServicesConnection());

  for (auto ep : client.ListEndpointPolicies(location.FullName())) {
    if (!ep) throw std::move(ep).status();
    std::cout << ep->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Network Services API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/products/networking
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/networkservices/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/networkservices
