# Connectors API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Connectors API][cloud-service-docs], a service that enables users to create and
manage connections to Google Cloud services and third-party business
applications.

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
#include "google/cloud/connectors/v1/connectors_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace connectors = ::google::cloud::connectors_v1;
  auto client =
      connectors::ConnectorsClient(connectors::MakeConnectorsConnection());

  for (auto c : client.ListConnections(location.FullName())) {
    if (!c) throw std::move(c).status();
    std::cout << c->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Connectors API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/apigee/docs/api-platform/connectors/about-connectors
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/connectors/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/connectors
