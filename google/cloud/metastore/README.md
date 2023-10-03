# Dataproc Metastore API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Dataproc Metastore API][cloud-service-docs], a service to manage the lifecycle
and configuration of metastore services.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/metastore/v1/dataproc_metastore_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  // Use the `-` wildcard to search in all locations.
  auto const location = google::cloud::Location(argv[1], "-");

  namespace metastore = ::google::cloud::metastore_v1;
  auto client = metastore::DataprocMetastoreClient(
      metastore::MakeDataprocMetastoreConnection());

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

- Official documentation about the [Dataproc Metastore API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/dataproc-metastore
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/metastore/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/metastore
