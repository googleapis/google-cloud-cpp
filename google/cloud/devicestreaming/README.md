# Device Streaming API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Device Streaming API][cloud-service-docs].

The Cloud API for device streaming usage.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/devicestreaming/v1/direct_access_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const project_id = std::string(argv[1]);
  auto const parent = "projects/" + project_id;

  namespace devicestreaming = ::google::cloud::devicestreaming_v1;
  auto client = devicestreaming::DirectAccessServiceClient(
      devicestreaming::MakeDirectAccessServiceConnection());

  for (auto r : client.ListDeviceSessions(parent)) {
    if (!r) throw std::move(r).status();
    std::cout << r->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Device Streaming API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/device-streaming/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/devicestreaming/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/devicestreaming
