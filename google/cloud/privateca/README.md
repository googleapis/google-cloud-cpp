# Certificate Authority API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Certificate Authority API][cloud-service-docs]. The Certificate Authority
Service API is a highly-available, scalable service that enables you to simplify
and automate the management of private certificate authorities (CAs) while
staying in control of your private keys.

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
#include "google/cloud/privateca/v1/certificate_authority_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace privateca = ::google::cloud::privateca_v1;
  auto client = privateca::CertificateAuthorityServiceClient(
      privateca::MakeCertificateAuthorityServiceConnection());

  for (auto ca_pool : client.ListCaPools(location.FullName())) {
    if (!ca_pool) throw std::move(ca_pool).status();
    std::cout << ca_pool->DebugString() << "\n";
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
  [Certificate Authority API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/certificate-authority-service/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/privateca/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/privateca
