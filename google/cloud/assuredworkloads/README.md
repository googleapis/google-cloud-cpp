# Assured Workloads API C++ Client Library

This directory contains an idiomatic C++ client library for
[Assured Workloads API][cloud-service-docs], a service to accelerate your path
to running more secure and compliant workloads on Google Cloud.

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
#include "google/cloud/assuredworkloads/v1/assured_workloads_client.h"
#include "google/cloud/location.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " organization-id location-id\n";
    return 1;
  }

  namespace assuredworkloads = ::google::cloud::assuredworkloads_v1;
  auto client = assuredworkloads::AssuredWorkloadsServiceClient(
      assuredworkloads::MakeAssuredWorkloadsServiceConnection());
  auto const parent =
      std::string("organizations/") + argv[1] + "/locations/" + argv[2];

  for (auto w : client.ListWorkloads(parent)) {
    if (!w) throw std::move(w).status();
    std::cout << w->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Assured Workloads API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/assured-workloads
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/assuredworkloads/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/assuredworkloads
