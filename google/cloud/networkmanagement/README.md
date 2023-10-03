# Network Management API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Network Management API][cloud-service-docs]. Provides a collection of network
performance monitoring and diagnostic capabilities.

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
#include "google/cloud/networkmanagement/v1/reachability_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "global");

  namespace networkmanagement = ::google::cloud::networkmanagement_v1;
  auto client = networkmanagement::ReachabilityServiceClient(
      networkmanagement::MakeReachabilityServiceConnection());

  for (auto t : client.ListConnectivityTests(location.FullName())) {
    if (!t) throw std::move(t).status();
    std::cout << t->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Network Management API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/network-intelligence-center/docs/connectivity-tests/concepts/overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/networkmanagement/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/networkmanagement
