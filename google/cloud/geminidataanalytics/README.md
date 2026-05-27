# Data Analytics API with Gemini C++ Client Library

This directory contains an idiomatic C++ client library for the
[Data Analytics API with Gemini][cloud-service-docs].

The Gemini Data Analytics API enables developers to build intelligent data
analytics applications. Leverage AI-powered chat interfaces to allow users to
interact with and analyze structured data using natural language.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/geminidataanalytics/v1/ EDIT HERE _client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id location-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace geminidataanalytics = ::google::cloud::geminidataanalytics_v1;
  auto client = geminidataanalytics::ServiceClient(
      geminidataanalytics::MakeServiceConnection());  // EDIT HERE

  for (auto r : client.List /*EDIT HERE*/ (location.FullName())) {
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

- Official documentation about the
  [Data Analytics API with Gemini][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/gemini/docs/conversational-analytics-api/overview
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/geminidataanalytics/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/geminidataanalytics
