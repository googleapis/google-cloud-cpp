# Cloud Speech-to-Text API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Speech-to-Text API][cloud-service-docs], a service which converts audio
to text by applying powerful neural network models.

While this library is **GA**, please note that the Google Cloud C++ client
libraries do **not** follow [Semantic Versioning](https://semver.org/).

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
#include "google/cloud/speech/v2/speech_client.h"
#include "google/cloud/project.h"
#include <iostream>

// Configure a simple recognizer for en-US.
void ConfigureRecognizer(google::cloud::speech::v2::RecognizeRequest& request) {
  *request.mutable_config()->add_language_codes() = "en-US";
  request.mutable_config()->set_model("short");
  *request.mutable_config()->mutable_auto_decoding_config() = {};
}

int main(int argc, char* argv[]) try {
  auto constexpr kDefaultUri = "gs://cloud-samples-data/speech/hello.wav";
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: " << argv[0] << " project <region>|global [gcs-uri]\n"
              << "  Specify the region desired or \"global\"\n"
              << "  The gcs-uri must be in gs://... format. It defaults to "
              << kDefaultUri << "\n";
    return 1;
  }
  std::string const project = argv[1];
  std::string location = argv[2];
  auto const uri = std::string{argc == 4 ? argv[3] : kDefaultUri};
  namespace speech = ::google::cloud::speech_v2;

  std::shared_ptr<speech::SpeechConnection> connection;
  google::cloud::speech::v2::RecognizeRequest request;
  ConfigureRecognizer(request);
  request.set_uri(uri);
  request.set_recognizer("projects/" + project + "/locations/" + location +
                         "/recognizers/_");

  if (location == "global") {
    // An empty location string indicates that the global endpoint of the
    // service should be used.
    location = "";
  }

  auto client = speech::SpeechClient(speech::MakeSpeechConnection(location));
  auto response = client.Recognize(request);
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

- Official documentation about the
  [Cloud Speech-to-Text API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/speech
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/speech/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/speech
