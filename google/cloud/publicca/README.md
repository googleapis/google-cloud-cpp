# Public Certificate Authority API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Public Certificate Authority API][cloud-service-docs].

The Public Certificate Authority API may be used to create and manage ACME
external account binding keys associated with Google Trust Services' publicly
trusted certificate authority.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/publicca/v1/public_certificate_authority_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }
  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace publicca = ::google::cloud::publicca_v1;
  auto client = publicca::PublicCertificateAuthorityServiceClient(
      publicca::MakePublicCertificateAuthorityServiceConnection());

  auto key = client.CreateExternalAccountKey(location.FullName(), {});
  if (!key) throw std::move(key).status();
  std::cout << "Success!\n";
  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the
  [Public Certificate Authority API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/certificate-manager/docs/public-ca
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/publicca/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/publicca
