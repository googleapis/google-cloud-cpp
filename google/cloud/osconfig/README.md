# OS Config API C++ Client Library

This directory contains an idiomatic C++ client library for the
[OS Config API][cloud-service-docs], a service for patch management, patch
compliance, and configuration management on VM instances.

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
#include "google/cloud/osconfig/v1/os_config_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace osconfig = ::google::cloud::osconfig_v1;
  auto client = osconfig::OsConfigServiceClient(
      osconfig::MakeOsConfigServiceConnection());

  auto const project = google::cloud::Project(argv[1]);
  for (auto p : client.ListPatchJobs(project.FullName())) {
    if (!p) throw std::move(p).status();
    std::cout << p->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [OS Config API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/compute/docs/os-configuration-management
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/osconfig/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/osconfig
