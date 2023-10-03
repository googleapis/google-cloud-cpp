# Cloud Dataplex API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Dataplex API][cloud-service-docs], an intelligent data fabric that helps
you unify distributed data and automate data management and governance across
that data to power analytics at scale.

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
#include "google/cloud/dataplex/v1/dataplex_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace dataplex = ::google::cloud::dataplex_v1;
  auto client = dataplex::DataplexServiceClient(
      dataplex::MakeDataplexServiceConnection());

  for (auto l : client.ListLakes(location.FullName())) {
    if (!l) throw std::move(l).status();
    std::cout << l->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Dataplex API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/dataplex
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/dataplex/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/dataplex
