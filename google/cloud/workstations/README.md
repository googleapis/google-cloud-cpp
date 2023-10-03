# Cloud Workstations API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Workstations API][cloud-service-docs], a service that provides
preconfigured, customizable, and secure managed development environments on
Google Cloud.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/workstations/v1/workstations_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace workstations = ::google::cloud::workstations_v1;
  auto client = workstations::WorkstationsClient(
      workstations::MakeWorkstationsConnection());

  for (auto wc : client.ListWorkstationClusters(location.FullName())) {
    if (!wc) throw std::move(wc).status();
    std::cout << wc->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Workstations API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/workstations
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/workstations/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/workstations
