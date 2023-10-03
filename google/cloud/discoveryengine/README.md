# Discovery Engine API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Discovery Engine API][cloud-service-docs], a service that powers high-quality
content recommendations on your digital properties.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/discoveryengine/v1/document_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], "global");

  namespace discoveryengine = ::google::cloud::discoveryengine_v1;
  auto client = discoveryengine::DocumentServiceClient(
      discoveryengine::MakeDocumentServiceConnection());

  auto const parent = location.FullName() +
                      "/dataStores/default_data_store/branches/default_branch";
  for (auto d : client.ListDocuments(parent)) {
    if (!d) throw std::move(d).status();
    std::cout << d->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Discovery Engine API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/discovery-engine
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/discoveryengine/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/discoveryengine
