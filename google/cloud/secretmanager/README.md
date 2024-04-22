# Secret Manager API C++ Client Library

This directory contains an idiomatic C++ client library for
[Secret Manager API][cloud-service-root], a service that stores sensitive data
such as API keys, passwords, and certificates. Provides convenience while
improving security.

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
#include "google/cloud/secretmanager/v1/secret_manager_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace secretmanager = ::google::cloud::secretmanager_v1;
  auto client = secretmanager::SecretManagerServiceClient(
      secretmanager::MakeSecretManagerServiceConnection());

  auto const parent = std::string("projects/") + argv[1];
  for (auto secret : client.ListSecrets(parent)) {
    if (!secret) throw std::move(secret).status();
    std::cout << secret->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Secret Manager API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/secret-manager/docs/overview
[cloud-service-root]: https://cloud.google.com/secret-manager
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/secretmanager/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/secretmanager
