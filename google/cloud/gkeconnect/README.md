# Connect Gateway API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Connect Gateway API][cloud-service-docs].

The Connect Gateway service allows connectivity from external parties to
connected Kubernetes clusters.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/gkeconnect/gateway/v1/gateway_control_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " name\n";
    return 1;
  }

  namespace gkeconnect = ::google::cloud::gkeconnect_gateway_v1;
  auto client = gkeconnect::GatewayControlClient(
      gkeconnect::MakeGatewayControlConnection());

  google::cloud::gkeconnect::gateway::v1::GenerateCredentialsRequest request;
  request.set_name(argv[1]);

  auto response = client.GenerateCredentials(request);
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

- Official documentation about the [Connect Gateway API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/kubernetes-engine/enterprise/multicluster-management/gateway
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/gkeconnect/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/gkeconnect
