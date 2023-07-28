# Web Security Scanner API C++ Client Library

This directory contains an idiomatic C++ client library for
[Web Security Scanner][cloud-service-docs], a service that scans your Compute
and App Engine apps for common web vulnerabilities.

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
#include "google/cloud/websecurityscanner/v1/web_security_scanner_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace websecurityscanner = ::google::cloud::websecurityscanner_v1;
  auto client = websecurityscanner::WebSecurityScannerClient(
      websecurityscanner::MakeWebSecurityScannerConnection());
  auto const project = google::cloud::Project(argv[1]);
  google::cloud::websecurityscanner::v1::ListScanConfigsRequest request;
  request.set_parent(project.FullName());
  for (auto c : client.ListScanConfigs(request)) {
    if (!c) throw std::move(c).status();
    std::cout << c->DebugString() << "\n";
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
  [Web Security Scanner API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/security-command-center
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/websecurityscanner/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/websecurityscanner
