# Service Directory API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Service Directory][cloud-service], a platform for discovering, publishing, and
connecting services, regardless of the environment.

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
#include "google/cloud/servicedirectory/v1/registration_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace servicedirectory = ::google::cloud::servicedirectory_v1;
  auto client = servicedirectory::RegistrationServiceClient(
      servicedirectory::MakeRegistrationServiceConnection());

  for (auto ns : client.ListNamespaces(location.FullName())) {
    if (!ns) throw std::move(ns).status();
    std::cout << ns->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Service Directory API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/service-directory
[cloud-service-docs]: https://cloud.google.com/service-directory/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/servicedirectory/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/servicedirectory
