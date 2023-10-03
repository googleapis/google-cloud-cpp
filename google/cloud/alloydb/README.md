# AlloyDB API C++ Client Library

This directory contains an idiomatic C++ client library for the
[AlloyDB API][cloud-service-docs]. AlloyDB for PostgreSQL is an open
source-compatible database service that provides a powerful option for
migrating, modernizing, or building commercial-grade applications. It offers
full compatibility with standard PostgreSQL, and is more than 4x faster for
transactional workloads and up to 100x faster for analytical queries than
standard PostgreSQL in our performance tests. AlloyDB for PostgreSQL offers a
99.99 percent availability SLA inclusive of maintenance.

AlloyDB is optimized for the most demanding use cases, allowing you to build new
applications that require high transaction throughput, large database sizes, or
multiple read resources; scale existing PostgreSQL workloads with no application
changes; and modernize legacy proprietary databases.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/alloydb/v1/alloy_db_admin_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "-");

  namespace alloydb = ::google::cloud::alloydb_v1;
  auto client =
      alloydb::AlloyDBAdminClient(alloydb::MakeAlloyDBAdminConnection());

  for (auto c : client.ListClusters(location.FullName())) {
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

- Official documentation about the [AlloyDB API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/alloydb
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/alloydb/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/alloydb
