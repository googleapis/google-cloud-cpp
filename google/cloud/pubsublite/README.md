# Cloud Pub/Sub Lite C++ Client Library

:construction:

This directory contains an idiomatic C++ client library for
[Cloud Pub/Sub Lite][cloud-service-root], a high-volume messaging service built
for very low cost of operation by offering zonal storage and pre-provisioned
capacity.

This library is **experimental**. Its APIs are subject to change without notice.

Please note that the Google Cloud C++ client libraries do **not** follow
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
#include "google/cloud/pubsublite/admin_client.h"
#include "google/cloud/pubsublite/endpoint.h"
#include "google/cloud/common_options.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace gc = ::google::cloud;
  namespace pubsublite = ::google::cloud::pubsublite;
  auto endpoint = pubsublite::EndpointFromZone(location.location_id());
  if (!endpoint) throw std::move(endpoint).status();
  auto client =
      pubsublite::AdminServiceClient(pubsublite::MakeAdminServiceConnection(
          gc::Options{}
              .set<gc::EndpointOption>(*endpoint)
              .set<gc::AuthorityOption>(*endpoint)));
  for (auto topic : client.ListTopics(location.FullName())) {
    if (!topic) throw std::move(topic).status();
    std::cout << topic->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Pub/Sub Lite][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/pubsub/lite/docs
[cloud-service-root]: https://cloud.google.com/pubsub/lite
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/pubsublite/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/pubsublite
