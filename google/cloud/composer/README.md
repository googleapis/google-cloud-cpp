# Cloud Composer C++ Client Library

This directory contains an idiomatic C++ client library for
[Cloud Composer][cloud-service], a service that manages Apache Airflow
environments on Google Cloud Platform.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/composer/environments_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace composer = ::google::cloud::composer;
  auto client =
      composer::EnvironmentsClient(composer::MakeEnvironmentsConnection());

  auto const parent =
      std::string("projects/") + argv[1] + "/locations/" + argv[2];
  for (auto e : client.ListEnvironments(parent)) {
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

- Official documentation about the [Cloud Composer API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/composer
[cloud-service-docs]: https://cloud.google.com/composer/docs
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-composer/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/composer
