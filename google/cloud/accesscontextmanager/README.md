# Access Context Manager API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Access Context Manager API][cloud-service-docs], a service for setting
attribute based access control on requests to GCP services.

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
#include "google/cloud/accesscontextmanager/v1/access_context_manager_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " policy-id\n";
    return 1;
  }

  namespace accesscontextmanager = ::google::cloud::accesscontextmanager_v1;
  auto client = accesscontextmanager::AccessContextManagerClient(
      accesscontextmanager::MakeAccessContextManagerConnection());

  auto const parent = std::string("accessPolicies/") + argv[1];
  for (auto r : client.ListAccessLevels(parent)) {
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
  [Access Context Manager API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/access-context-manager
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/accesscontextmanager/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/accesscontextmanager
