# Service Health API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Service Health API][cloud-service-docs], a service that helps you gain
visibility into disruptive events impacting Google Cloud products.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/servicehealth/v1/service_health_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "global");

  namespace servicehealth = ::google::cloud::servicehealth_v1;
  auto client = servicehealth::ServiceHealthClient(
      servicehealth::MakeServiceHealthConnection());

  for (auto e : client.ListEvents(location.FullName())) {
    if (!e) throw std::move(e).status();
    std::cout << e->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Service Health API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/service-health/docs/overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/servicehealth/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/servicehealth
