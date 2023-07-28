# Cloud Text-to-Speech API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Text-to-Speech API][cloud-service-docs], a service to synthesize
natural-sounding speech by applying powerful neural network models.

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
#include "google/cloud/texttospeech/v1/text_to_speech_client.h"
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

  namespace texttospeech = ::google::cloud::texttospeech_v1;
  auto client = texttospeech::TextToSpeechClient(
      texttospeech::MakeTextToSpeechConnection());

  google::cloud::texttospeech::v1::SynthesisInput input;
  input.set_text(kText);
  google::cloud::texttospeech::v1::VoiceSelectionParams voice;
  voice.set_language_code("en-US");
  google::cloud::texttospeech::v1::AudioConfig audio;
  audio.set_audio_encoding(google::cloud::texttospeech::v1::LINEAR16);

  auto response = client.SynthesizeSpeech(input, voice, audio);
  if (!response) throw std::move(response).status();
  // Normally one would play the results (response->audio_content()) over some
  // audio device. For this quickstart, we just print some information.
  auto constexpr kWavHeaderSize = 48;
  auto constexpr kBytesPerSample = 2;  // we asked for LINEAR16
  auto const sample_count =
      (response->audio_content().size() - kWavHeaderSize) / kBytesPerSample;
  std::cout << "The audio has " << sample_count << " samples\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the
  [Cloud Text-to-Speech API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/text-to-speech
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/texttospeech/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/texttospeech
