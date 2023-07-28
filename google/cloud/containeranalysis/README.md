# Container Analysis API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Container Analysis API][cloud-service-docs], an implementation of the
[Grafeas API](https://grafeas.io), which stores, and enables querying and
retrieval of critical metadata about all of your software artifacts.

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
#include "google/cloud/containeranalysis/v1/grafeas_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace containeranalysis = ::google::cloud::containeranalysis_v1;
  auto client = containeranalysis::GrafeasClient(
      containeranalysis::MakeGrafeasConnection());

  auto const project = google::cloud::Project(argv[1]);
  for (auto n : client.ListNotes(project.FullName(), /*filter=*/"")) {
    if (!n) throw std::move(n).status();
    std::cout << n->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Container Analysis API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/container-analysis
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/containeranalysis/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/containeranalysis
