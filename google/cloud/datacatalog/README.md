# Google Cloud Data Catalog API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Google Cloud Data Catalog API][cloud-service-docs], a fully managed and highly
scalable data discovery and metadata management service.

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
#include "google/cloud/datacatalog/v1/data_catalog_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace datacatalog = ::google::cloud::datacatalog_v1;
  auto client =
      datacatalog::DataCatalogClient(datacatalog::MakeDataCatalogConnection());

  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + argv[2];
  for (auto r : client.ListEntryGroups(parent)) {
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

- Official documentation about the [Google Cloud Data Catalog API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/data-catalog
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-datacatalog/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/datacatalog
