# Cloud Identity-Aware Proxy API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Identity-Aware Proxy API][cloud-service-docs], a service to control
access to applications running on Google Cloud Platform.

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
#include "google/cloud/iap/v1/identity_aware_proxy_o_auth_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace iap = ::google::cloud::iap_v1;
  auto client = iap::IdentityAwareProxyOAuthServiceClient(
      iap::MakeIdentityAwareProxyOAuthServiceConnection());

  google::cloud::iap::v1::ListBrandsRequest request;
  request.set_parent(google::cloud::Project(argv[1]).FullName());
  // ListBrands is not paginated, a single response includes all the "brands".
  auto response = client.ListBrands(request);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the
  [Cloud Identity-Aware Proxy API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/iap
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/iap/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/iap
