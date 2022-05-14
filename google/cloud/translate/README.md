# Cloud Translation API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Translation API][cloud-service-docs], a service to integrate text
translation into your website or application.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

## Supported Platforms

* Windows, macOS, Linux
* C++11 (and higher) compilers (we test with GCC >= 5.4, Clang >= 6.0, and
  MSVC >= 2017)
* Environments with or without exceptions
* Bazel (>= 4.0) and CMake (>= 3.5) builds

## Documentation

* Official documentation about the [Cloud Translation API][cloud-service-docs] service
* [Reference doxygen documentation][doxygen-link] for each release of this
  client library
* Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/translate
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-translate/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/translate

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->
```cc
#include "google/cloud/translate/translation_client.h"
#include "google/cloud/project.h"
#include <iostream>
#include <stdexcept>

auto constexpr kText = R"""(
Four score and seven years ago our fathers brought forth on this
continent, a new nation, conceived in Liberty, and dedicated to
the proposition that all men are created equal.)""";

int main(int argc, char* argv[]) try {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " project-id "
              << "[target-language (default: es-419)]\n";
    return 1;
  }

  namespace translate = ::google::cloud::translate;
  auto client = translate::TranslationServiceClient(
      translate::MakeTranslationServiceConnection());

  auto const project = google::cloud::Project(argv[1]);
  auto const target = std::string{argc >= 3 ? argv[2] : "es-419"};
  auto response =
      client.TranslateText(project.FullName(), target, {std::string{kText}});
  if (!response) throw std::runtime_error(response.status().message());

  for (auto const& translation : response->translations()) {
    std::cout << translation.translated_text() << "\n";
  }

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
