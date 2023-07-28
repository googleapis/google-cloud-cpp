# Service Usage API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Service Usage API][cloud-service], an infrastructure service of Google Cloud
that lets you list and manage APIs and Services in your Cloud projects.

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
#include "google/cloud/serviceusage/v1/service_usage_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace serviceusage = ::google::cloud::serviceusage_v1;
  auto client = serviceusage::ServiceUsageClient(
      serviceusage::MakeServiceUsageConnection());

  auto const project = google::cloud::Project(argv[1]);
  google::api::serviceusage::v1::ListServicesRequest request;
  request.set_parent(project.FullName());
  request.set_filter("state:ENABLED");
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

- Official documentation about the [Service Usage API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/service-usage
[cloud-service-docs]: https://cloud.google.com/service-usage/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/serviceusage/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/serviceusage
