# Observability API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Observability API][cloud-service-docs].

Provides functionality for configuring the observability scope, which controls
the log, metric, and trace data that you can view.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/observability/v1/observability_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace observability = ::google::cloud::observability_v1;
  auto client = observability::ObservabilityClient(
      observability::MakeObservabilityConnection());

  for (auto const& r : client.ListBuckets(location.FullName())) {
    if (!r) throw google::cloud::Status(r.status());
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

- Official documentation about the [Observability API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/stackdriver/docs/
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/observability/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/observability
