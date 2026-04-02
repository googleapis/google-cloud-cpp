# Workload Manager API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Workload Manager API][cloud-service-docs].

Workload Manager is a service that provides tooling for enterprise workloads to
automate the deployment and validation of your workloads against best practices
and recommendations.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/workloadmanager/v1/workload_manager_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace workloadmanager = ::google::cloud::workloadmanager_v1;
  auto client = workloadmanager::WorkloadManagerClient(
      workloadmanager::MakeWorkloadManagerConnection());

  for (auto r : client.ListEvaluations(location.FullName())) {
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

- Official documentation about the [Workload Manager API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://docs.cloud.google.com/workload-manager/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/workloadmanager/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/workloadmanager
