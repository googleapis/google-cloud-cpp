# Cloud Functions API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Functions API][cloud-service-docs], a lightweight compute solution for
developers to create single-purpose, stand-alone functions that respond to Cloud
events without the need to manage a server or runtime environment.

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
#include "google/cloud/functions/v2/function_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "-");

  namespace functions = ::google::cloud::functions_v2;
  auto client = functions::FunctionServiceClient(
      functions::MakeFunctionServiceConnection());

  for (auto f : client.ListFunctions(location.FullName())) {
    if (!f) throw std::move(f).status();
    std::cout << f->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Functions API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/functions
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/functions/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/functions
