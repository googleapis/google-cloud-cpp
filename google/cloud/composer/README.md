# Cloud Composer C++ Client Library

This directory contains an idiomatic C++ client library for
[Cloud Composer][cloud-service], a service that manages Apache Airflow
environments on Google Cloud Platform.

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
#include "google/cloud/composer/v1/environments_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace composer = ::google::cloud::composer_v1;
  auto client =
      composer::EnvironmentsClient(composer::MakeEnvironmentsConnection());

  for (auto e : client.ListEnvironments(location.FullName())) {
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

- Official documentation about the [Cloud Composer API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/composer
[cloud-service-docs]: https://cloud.google.com/composer/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/composer/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/composer
