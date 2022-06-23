# Web Risk API C++ Client Library

This directory contains an idiomatic C++ client library for
[Web Risk][cloud-service-docs], a service to detect malicious URLs on your
website and in client applications.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++14 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Web Risk API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/web-risk
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-webrisk/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/webrisk

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/webrisk/web_risk_client.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0]
              << " [uri (default https://www.google.com)]\n";
    return 1;
  }

  namespace webrisk = ::google::cloud::webrisk;
  auto client =
      webrisk::WebRiskServiceClient(webrisk::MakeWebRiskServiceConnection());

  auto const uri = std::string{argc == 2 ? argv[1] : "https://www.google.com/"};
  auto const threat_types = std::vector<webrisk::v1::ThreatType>{
      webrisk::v1::MALWARE, webrisk::v1::UNWANTED_SOFTWARE};
  auto response = client.SearchUris("https://www.google.com/", threat_types);
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
