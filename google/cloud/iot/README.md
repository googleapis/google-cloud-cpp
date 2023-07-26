# Cloud IoT API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud IoT API][cloud-service-docs], a service to register and manage IoT
(Internet of Things) devices that connect to the Google Cloud Platform.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

For detailed instructions on how to build and install this library, see the
top-level [README](/README.md#building-and-installing).

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/iot/v1/device_manager_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  namespace iot = ::google::cloud::iot_v1;
  auto client = iot::DeviceManagerClient(iot::MakeDeviceManagerConnection());

  auto const project = google::cloud::Project(argv[1]);
  auto const parent = project.FullName() + "/locations/" + argv[2];
  for (auto dr : client.ListDeviceRegistries(parent)) {
    if (!dr) throw std::move(dr).status();
    std::cout << dr->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud IoT API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/iot
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/iot/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/iot
