# Security Center Management API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Security Center Management API][cloud-service-docs], a built-in security and
risk management solution for Google Cloud. Use this API to programmatically
update the settings and configuration of Security Command Center.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/securitycentermanagement/v1/security_center_management_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "global");

  namespace securitycentermanagement =
      ::google::cloud::securitycentermanagement_v1;
  auto client = securitycentermanagement::SecurityCenterManagementClient(
      securitycentermanagement::MakeSecurityCenterManagementConnection());

  for (auto m :
       client.ListEventThreatDetectionCustomModules(location.FullName())) {
    if (!m) throw std::move(m).status();
    std::cout << m->DebugString() << "\n";
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
  [Security Center Management API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/security-command-center/docs/reference/security-center-management/rest
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/securitycentermanagement/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/securitycentermanagement
