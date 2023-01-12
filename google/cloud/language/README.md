# Cloud Natural Language API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Natural Language API][cloud-service-docs], a service that provides
natural language understanding technologies, such as sentiment analysis, entity
recognition, entity sentiment analysis, and other text annotations.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
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
#include "google/cloud/language/language_client.h"
#include <iostream>

auto constexpr kText = R"""(
Four score and seven years ago our fathers brought forth on this
continent, a new nation, conceived in Liberty, and dedicated to
the proposition that all men are created equal.)""";

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n";
    return 1;
  }

  namespace language = ::google::cloud::language;
  auto client = language::LanguageServiceClient(
      language::MakeLanguageServiceConnection());

  language::v1::Document document;
  document.set_type(language::v1::Document::PLAIN_TEXT);
  document.set_content(kText);
  document.set_language("en-US");

  auto response = client.AnalyzeEntities(document);
  if (!response) throw std::move(response).status();

  for (auto const& entity : response->entities()) {
    if (entity.type() != language::v1::Entity::NUMBER) continue;
    std::cout << entity.DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Natural Language API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/natural-language
[doxygen-link]: https://googleapis.dev/cpp/google-cloud-language/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/language
