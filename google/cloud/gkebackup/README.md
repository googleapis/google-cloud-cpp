# Backup for GKE API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Backup for GKE API][cloud-service-docs], a service to Backup for GKE is a managed Kubernetes workload backup and restore service for GKE clusters.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/gkebackup/ EDIT HERE .h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace gkebackup = ::google::cloud::gkebackup;
  auto client = gkebackup::Client(gkebackup::MakeConnection());

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

- Official documentation about the [Backup for GKE API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/gkebackup
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/gkebackup/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/gkebackup
