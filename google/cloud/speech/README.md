# Cloud Speech-to-Text API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Speech-to-Text API][cloud-service-docs], a service which converts audio
to text by applying powerful neural network models.

While this library is **GA**, please note that the Google Cloud C++ client libraries do **not** follow
[Semantic Versioning](https://semver.org/).

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
#include "google/cloud/speech/v1/speech_client.h"
#include "google/cloud/project.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  auto constexpr kDefaultUri = "gs://cloud-samples-data/speech/hello.wav";
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0] << " [gcs-uri]\n"
              << "  The gcs-uri must be in gs://... format. It defaults to "
              << kDefaultUri << "\n";
    return 1;
  }
  auto uri = std::string{argc == 2 ? argv[1] : kDefaultUri};

  namespace speech = ::google::cloud::speech_v1;
  auto client = speech::SpeechClient(speech::MakeSpeechConnection());

  google::cloud::speech::v1::RecognitionConfig config;
  config.set_language_code("en-US");
  google::cloud::speech::v1::RecognitionAudio audio;
  audio.set_uri(uri);
  auto response = client.Recognize(config, audio);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Speech-to-Text API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/speech
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/speech/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/speech
