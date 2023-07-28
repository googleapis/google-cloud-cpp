# Service Management API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Service Management API][cloud-service-docs]. This service allows service
producers to publish their services on Google Cloud Platform so that they can be
discovered and used by service consumers.

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
#include "google/cloud/servicemanagement/v1/service_manager_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace servicemanagement = ::google::cloud::servicemanagement_v1;
  auto client = servicemanagement::ServiceManagerClient(
      servicemanagement::MakeServiceManagerConnection());

  google::api::servicemanagement::v1::ListServicesRequest request;
  request.set_producer_project_id(argv[1]);
  for (auto s : client.ListServices(request)) {
    if (!s) throw std::move(s).status();
    std::cout << s->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Service Management API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/service-management
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/servicemanagement/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/servicemanagement
