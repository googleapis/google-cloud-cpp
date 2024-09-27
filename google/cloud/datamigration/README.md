# Database Migration API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Database Migration API][cloud-service-docs], a service to simplify migrations
to Cloud SQL.

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
#include "google/cloud/datamigration/v1/data_migration_client.h"
#include "google/cloud/location.h"
#include <iostream>
int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }
  auto const location = google::cloud::Location(argv[1], argv[2]);
  namespace datamigration = ::google::cloud::datamigration_v1;
  auto client = datamigration::DataMigrationServiceClient(
      datamigration::MakeDataMigrationServiceConnection());

  std::cout << "------------------------TEST "
               "ListMigrationJobs------------------------"
            << std::endl;
  for (auto mj : client.ListMigrationJobs(location.FullName())) {
    if (!mj) throw std::move(mj).status();
    std::cout << mj->DebugString() << "\n";
  }

  std::cout
      << "------------------------TEST ListLocations------------------------"
      << std::endl;
  google::cloud::location::ListLocationsRequest ll_req;
  *ll_req.mutable_name() = "projects/" + std::string(argv[1]);
  std::cout << "ListLocationsRequest name = " << ll_req.name() << "\n";
  for (auto l : client.ListLocations(ll_req)) {
    if (!l) throw std::move(l).status();
    std::cout << l->DebugString() << "\n";
  }

  std::cout
      << "------------------------TEST GetLocation------------------------"
      << std::endl;
  google::cloud::location::GetLocationRequest gl_req;
  *gl_req.mutable_name() =
      "projects/" + std::string(argv[1]) + "/locations/" + argv[2];
  google::cloud::StatusOr<google::cloud::location::Location> gl_res =
      client.GetLocation(gl_req);
  if (!gl_res) throw std::move(gl_res).status();
  std::cout << gl_res->DebugString() << "\n";

  std::cout
      << "------------------------TEST ListOperations------------------------"
      << std::endl;

  for (auto l :
       client.ListOperations("projects/" + std::string(argv[1]) +
                                 "/locations/" + argv[2] + "/operations",
                             "")) {
    if (!l) throw std::move(l).status();
    std::cout << l->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Database Migration API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/database-migration
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/datamigration/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/datamigration
