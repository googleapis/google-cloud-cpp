# Oracle Database@Google Cloud API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Oracle Database@Google Cloud API][cloud-service-docs].

The Oracle Database@Google Cloud API provides a set of APIs to manage Oracle
database services, such as Exadata and Autonomous Databases.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/oracledatabase/v1/oracle_database_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace oracledatabase = ::google::cloud::oracledatabase_v1;
  auto client = oracledatabase::OracleDatabaseClient(
      oracledatabase::MakeOracleDatabaseConnection());

  for (auto r : client.ListCloudExadataInfrastructures(location.FullName())) {
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

- Official documentation about the
  [Oracle Database@Google Cloud API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/oracle/database/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/oracledatabase/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/oracledatabase
