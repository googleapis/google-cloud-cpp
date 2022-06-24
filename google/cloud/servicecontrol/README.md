# Service Control API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Service Control API][cloud-service-docs]. This service provides admission
control and telemetry reporting for services integrated with Service
Infrastructure.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++14 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Service Control API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/service-control
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-servicecontrol/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/servicecontrol

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/servicecontrol/service_controller_client.h"
#include "google/cloud/project.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace servicecontrol = ::google::cloud::servicecontrol;
  auto client = servicecontrol::ServiceControllerClient(
      servicecontrol::MakeServiceControllerConnection());

  auto const project = google::cloud::Project(argv[1]);
  google::api::servicecontrol::v1::CheckRequest request;
  request.set_service_name("pubsub.googleapis.com");
  *request.mutable_operation() = [&] {
    using ::google::protobuf::util::TimeUtil;

    google::api::servicecontrol::v1::Operation op;
    op.set_operation_id("TODO-use-UUID-4-or-UUID-5");
    op.set_operation_name("google.pubsub.v1.Publisher.Publish");
    op.set_consumer_id(project.FullName());
    *op.mutable_start_time() = TimeUtil::GetCurrentTime();
    return op;
  }();

  auto response = client.Check(request);
  if (!response) throw std::runtime_error(response.status().message());
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
```
<!-- inject-quickstart-end -->

* Packaging maintainers or developers who prefer to install the library in a
  fixed directory (such as `/usr/local` or `/opt`) should consult the
  [packaging guide](/doc/packaging.md).
* Developers wanting to use the libraries as part of a larger CMake or Bazel
  project should consult the [quickstart guides](#quickstart) for the library
  or libraries they want to use.
* Developers wanting to compile the library just to run some of the examples or
  tests should read the current document.
* Contributors and developers to `google-cloud-cpp` should consult the guide to
  [setup a development workstation][howto-setup-dev-workstation].

[howto-setup-dev-workstation]: /doc/contributor/howto-guide-setup-development-workstation.md

## Contributing changes

See [`CONTRIBUTING.md`](/CONTRIBUTING.md) for details on how to
contribute to this project, including how to build and test your changes
as well as how to properly format your code.

## Licensing

Apache 2.0; see [`LICENSE`](/LICENSE) for details.
