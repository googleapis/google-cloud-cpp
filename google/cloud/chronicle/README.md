# Chronicle API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Chronicle API][cloud-service-docs].

The Chronicle API serves all customer endpoints.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/chronicle/v1/entity_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " project-id location-id instance-id\n";
    return 1;
  }
  auto const endpoint = "us-chronicle.googleapis.com";
  auto const location = google::cloud::Location(argv[1], argv[2]);
  auto const instance_id = std::string(argv[3]);

  namespace gc = ::google::cloud;
  namespace chronicle = ::google::cloud::chronicle_v1;

  auto client =
      chronicle::EntityServiceClient(chronicle::MakeEntityServiceConnection(
          gc::Options{}
              .set<gc::EndpointOption>(endpoint)
              .set<gc::AuthorityOption>(endpoint)));

  for (auto r : client.ListWatchlists(location.FullName() + "/instances/" +
                                      instance_id)) {
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

- Official documentation about the [Chronicle API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/chronicle/docs/secops/secops-overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/chronicle/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/chronicle
