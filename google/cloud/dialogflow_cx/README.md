# Dialogflow CX API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Dialogflow CX API][cloud-service-docs], a service to build conversational
interfaces (for example, chatbots, and voice-powered apps and devices). There
are [two editions] of Dialogflow, this library supports the CX edition.

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
#include "google/cloud/dialogflow_cx/agents_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " project-id region-id\n";
    return 1;
  }

  auto const location = google::cloud::Location(argv[1], argv[2]);

  namespace dialogflow_cx = ::google::cloud::dialogflow_cx;
  auto client = dialogflow_cx::AgentsClient(
      dialogflow_cx::MakeAgentsConnection(location.location_id()));

  for (auto a : client.ListAgents(location.FullName())) {
    if (!a) throw std::move(a).status();
    std::cout << a->DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Dialogflow API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/dialogflow/cx/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/dialogflow_cx/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/dialogflow_cx
[two editions]: https://cloud.google.com/dialogflow/docs/editions
