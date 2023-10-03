# Cloud TPU API C++ Client Library

This directory contains an idiomatic C++ client library for
[Cloud TPU][cloud-service], a service that provides customers with access to
Google TPU technology.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/tpu/v2/tpu_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "-");

  namespace tpu = ::google::cloud::tpu_v2;
  auto client = tpu::TpuClient(tpu::MakeTpuConnection());

  for (auto n : client.ListNodes(location.FullName())) {
    if (!n) throw std::move(n).status();
    std::cout << n->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud TPU API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/tpu
[cloud-service-docs]: https://cloud.google.com/tpu/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/tpu/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/tpu
