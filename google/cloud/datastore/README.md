# Cloud Datastore API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Datastore API][cloud-service-docs], a service to Accesses the schemaless
NoSQL database to provide fully managed, robust, scalable storage for your
application.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/datastore/ EDIT HERE .h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace datastore = ::google::cloud::datastore;
  auto client = datastore::Client(datastore::MakeConnection());

  auto const project = google::cloud::Project(argv[1]);
  for (auto r : client.List /*EDIT HERE*/ (project.FullName())) {
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

- Official documentation about the [Cloud Datastore API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/datastore
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/datastore/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/datastore
