# Apigee Connect API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Apigee Hybrid][cloud-service-docs] management APIs. Apigee Hybrid is a platform
for developing and managing API proxies that features a hybrid deployment model.
The hybrid model includes a management plane hosted by Apigee in the Cloud and a
runtime plane that you install and manage on one of the
[supported Kubernetes platforms](https://cloud.google.com/apigee/docs/hybrid/supported-platforms).

<!-- TODO(#7900) - consider the lack of quickstart testing before GA -->

This library is **experimental**. Its APIs are subject to change without notice.

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
#include "google/cloud/apigeeconnect/v1/connection_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id endpoint-id\n";
    return 1;
  }

  namespace apigeeconnect = ::google::cloud::apigeeconnect_v1;
  auto client = apigeeconnect::ConnectionServiceClient(
      apigeeconnect::MakeConnectionServiceConnection());

  auto const parent =
      std::string{"projects/"} + argv[1] + "/endpoints/" + argv[2];
  for (auto r : client.ListConnections(parent)) {
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

- Official documentation about the [Apigee Connect API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/apigee/docs/hybrid/
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/apigeeconnect/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/apigeeconnect
