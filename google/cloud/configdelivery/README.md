# Config Delivery API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Config Delivery API][cloud-service-docs].

ConfigDelivery service manages the deployment of kubernetes configuration to a
fleet of kubernetes clusters.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/configdelivery/v1/config_delivery_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace configdelivery = ::google::cloud::configdelivery_v1;
  auto client = configdelivery::ConfigDeliveryClient(
      configdelivery::MakeConfigDeliveryConnection());

  for (auto r : client.ListFleetPackages(location.FullName())) {
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

- Official documentation about the [Config Delivery API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/kubernetes-engine/enterprise/config-sync/docs/concepts/fleet-packages
[doxygen-link]: https://cloud.google.com/kubernetes-engine/enterprise/config-sync/docs/concepts/fleet-packages
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/configdelivery
