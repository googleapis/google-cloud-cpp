# Generative Language API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Generative Language API][cloud-service-docs].

The Gemini API allows developers to build generative AI applications using
Gemini models. Gemini is our most capable model, built from the ground up to be
multimodal. It can generalize and seamlessly understand, operate across, and
combine different types of information including language, images, audio, video,
and code. You can use the Gemini API for use cases like reasoning across text
and images, content generation, dialogue agents, summarization and
classification systems, and more.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

## Quickstart

The [quickstart/](quickstart/README.md) directory contains a minimal environment
to get started using this client library in a larger project. The following
"Hello World" program is used in this quickstart, and should give you a taste of
this library.

<!-- inject-quickstart-start -->

```cc
#include "google/cloud/generativelanguage/v1/model_client.h"
#include "google/cloud/location.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << "\n";
    return 1;
  }

  namespace generativelanguage = ::google::cloud::generativelanguage_v1;
  auto client = generativelanguage::ModelServiceClient(
      generativelanguage::MakeModelServiceConnection());

  for (auto r : client.ListModels({})) {
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

- Official documentation about the [Generative Language API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://ai.google.dev/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/generativelanguage/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/generativelanguage
