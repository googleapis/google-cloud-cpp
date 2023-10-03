# BeyondCorp API C++ Client Library

This directory contains an idiomatic C++ client library for the
[BeyondCorp API][cloud-service-docs] which provides identity and context aware
access controls for enterprise resources and enables zero-trust access. Using
the BeyondCorp Enterprise APIs, enterprises can set up multi-cloud and on-prem
connectivity using the App Connector hybrid connectivity solution.

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
#include "google/cloud/beyondcorp/appconnectors/v1/app_connectors_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace appconnectors = ::google::cloud::beyondcorp_appconnectors_v1;
  auto client = appconnectors::AppConnectorsServiceClient(
      appconnectors::MakeAppConnectorsServiceConnection());

  for (auto ac : client.ListAppConnectors(location.FullName())) {
    if (!ac) throw std::move(ac).status();
    std::cout << ac->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [BeyondCorp API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/beyondcorp
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/beyondcorp/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/beyondcorp
