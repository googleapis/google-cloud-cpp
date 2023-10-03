# Cloud Run Admin API C++ Client Library

This directory contains an idiomatic C++ client library for
[Cloud Run][cloud-service-root], a managed compute platform that lets you run
containers directly on top of Google's scalable infrastructure.

Note that this library only provides tools to **manage** Cloud Run resources. To
actually deploy a C++ function to Cloud Run, see the
[Cloud Run for C++ Hello World][hello-world] example, which uses the
[C++ Functions Framework][functions-framework] library.

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
#include "google/cloud/run/v2/services_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace run = ::google::cloud::run_v2;
  auto client = run::ServicesClient(run::MakeServicesConnection());

  for (auto s : client.ListServices(location.FullName())) {
    if (!s) throw std::move(s).status();
    std::cout << s->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Run Admin API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/run/docs
[cloud-service-root]: https://cloud.google.com/run
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/run/latest/
[functions-framework]: https://github.com/GoogleCloudPlatform/functions-framework-cpp
[hello-world]: https://github.com/GoogleCloudPlatform/cpp-samples/tree/main/cloud-run-hello-world
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/run
