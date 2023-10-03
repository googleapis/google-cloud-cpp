# Workflow Executions API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Workflows API][cloud-service-docs], a service to orchestrate and automate
Google Cloud and HTTP-based API services with serverless workflows.

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
#include "google/cloud/workflows/v1/workflows_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace workflows = ::google::cloud::workflows_v1;
  auto client =
      workflows::WorkflowsClient(workflows::MakeWorkflowsConnection());

  for (auto w : client.ListWorkflows(location.FullName())) {
    if (!w) throw std::move(w).status();
    std::cout << w->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Workflow Executions API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/workflows
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/workflows/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/workflows
