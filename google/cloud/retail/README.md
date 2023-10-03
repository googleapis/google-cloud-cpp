# Retail API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Retail API][cloud-service-docs]; Cloud Retail service enables customers to
build end-to-end personalized recommendation systems without requiring a high
level of expertise in machine learning, recommendation system, or Google Cloud.

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
#include "google/cloud/retail/v2/catalog_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  // The service only accepts "global" as the location for ListCatalogs()
  auto const location = google::cloud::Location(argv[1], "global");

  namespace retail = ::google::cloud::retail_v2;
  auto client =
      retail::CatalogServiceClient(retail::MakeCatalogServiceConnection());

  for (auto c : client.ListCatalogs(location.FullName())) {
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

- Official documentation about the [Retail API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/retail/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/retail/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/retail
