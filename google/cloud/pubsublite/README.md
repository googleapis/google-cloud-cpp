# Cloud Pub/Sub Lite C++ Client Library

:construction:

This directory contains an idiomatic C++ client library for
[Cloud Pub/Sub Lite][cloud-service-root], a high-volume
messaging service built for very low cost of operation by offering zonal storage
and pre-provisioned capacity.

This library is **experimental**. Its APIs are subject to change without notice.

Please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

- Windows, macOS, Linux
- C++14 (and higher) compilers (we test with GCC >= 7.3, Clang >= 6.0, and
  MSVC >= 2017)
- Environments with or without exceptions
- Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

- Official documentation about the [Cloud Pub/Sub Lite][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/pubsublite/admin_client.h"
#include "google/cloud/pubsublite/endpoint.h"
#include "google/cloud/common_options.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id zone-id\n";
    return 1;
  }

  namespace gc = ::google::cloud;
  namespace pubsublite = ::google::cloud::pubsublite;
  auto const zone_id = std::string{argv[2]};
  auto endpoint = pubsublite::EndpointFromZone(zone_id);
  if (!endpoint) throw std::move(endpoint).status();
  auto client =
      pubsublite::AdminServiceClient(pubsublite::MakeAdminServiceConnection(
          gc::Options{}
              .set<gc::EndpointOption>(*endpoint)
              .set<gc::AuthorityOption>(*endpoint)));
  auto const parent =
      std::string{"projects/"} + argv[1] + "/locations/" + zone_id;
  for (auto topic : client.ListTopics(parent)) {
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

- Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
- Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
- Developers wanting to compile the library just to run some examples or
  tests should read the current document.
- Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.

[cloud-service-docs]: https://cloud.google.com/pubsub/lite/docs
[cloud-service-root]: https://cloud.google.com/pubsub/lite
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-pubsublite/latest/
[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/pubsublite
