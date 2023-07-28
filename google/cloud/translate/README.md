# Cloud Translation API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Translation API][cloud-service-docs], a service to integrate text
translation into your website or application.

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
#include "google/cloud/translate/v3/translation_client.h"
#include "google/cloud/project.h"
#include <iostream>

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

  namespace translate = ::google::cloud::translate_v3;
  auto client = translate::TranslationServiceClient(
      translate::MakeTranslationServiceConnection());

  auto const project = google::cloud::Project(argv[1]);
  auto const target = std::string{argc >= 3 ? argv[2] : "es-419"};
  auto response =
      client.TranslateText(project.FullName(), target, {std::string{kText}});
  if (!response) throw std::move(response).status();

  for (auto const& translation : response->translations()) {
    std::cout << translation.translated_text() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Translation API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/translate
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/translate/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/translate
