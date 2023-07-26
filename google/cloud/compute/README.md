# Compute Engine API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Compute Engine API][cloud-service-docs], a service that lets you create and run
virtual machines on Google’s infrastructure.

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/compute/disks/v1/disks_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }

  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(
      google::cloud::ExperimentalTag{},
      disks::MakeDisksConnectionRest(google::cloud::ExperimentalTag{}));

  for (auto const& disk : client.ListDisks(argv[1], argv[2])) {
    if (!disk) throw std::move(disk).status();
    std::cout << disk->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Compute Engine API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/compute
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/compute/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/compute
