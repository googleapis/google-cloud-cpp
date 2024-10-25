# Cloud Video Intelligence API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Video Intelligence API][cloud-service], a service to detect objects,
explicit content, and scene changes in videos. It also specifies the region for
annotation and transcribes speech to text.

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
#include "google/cloud/videointelligence/v1/video_intelligence_client.h"
#include "google/protobuf/util/time_util.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  auto constexpr kDefaultUri = "gs://cloud-samples-data/video/animals.mp4";
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0] << " [video-uri]\n"
              << "  The gcs-uri must be in gs://... format and must point to a"
              << " MP4 video.\n"
              << "It defaults to " << kDefaultUri << "\n";
    return 1;
  }
  auto uri = std::string{argc == 2 ? argv[1] : kDefaultUri};

  namespace videointelligence = ::google::cloud::videointelligence_v1;
  auto client = videointelligence::VideoIntelligenceServiceClient(
      videointelligence::MakeVideoIntelligenceServiceConnection());

  using google::cloud::videointelligence::v1::Feature;
  google::cloud::videointelligence::v1::AnnotateVideoRequest request;
  request.set_input_uri(uri);
  request.add_features(Feature::SPEECH_TRANSCRIPTION);
  auto& config =
      *request.mutable_video_context()->mutable_speech_transcription_config();
  config.set_language_code("en-US");  // Adjust based
  config.set_max_alternatives(1);  // We will just print the highest-confidence

  auto future = client.AnnotateVideo(request);
  std::cout << "Waiting for response";
  auto const delay = std::chrono::seconds(5);
  while (future.wait_for(delay) == std::future_status::timeout) {
    std::cout << '.' << std::flush;
  }
  std::cout << "DONE\n";

  auto response = future.get();
  if (!response) throw std::move(response).status();

  for (auto const& result : response->annotation_results()) {
    using ::google::protobuf::util::TimeUtil;
    std::cout << "Segment ["
              << TimeUtil::ToString(result.segment().start_time_offset())
              << ", " << TimeUtil::ToString(result.segment().end_time_offset())
              << "]\n";
    for (auto const& transcription : result.speech_transcriptions()) {
      if (transcription.alternatives().empty()) continue;
      std::cout << transcription.alternatives(0).transcript() << "\n";
    }
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
  [Cloud Video Intelligence API][cloud-service-docs] service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service]: https://cloud.google.com/video-intelligence
[cloud-service-docs]: https://cloud.google.com/video-intelligence/docs
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/videointelligence/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/videointelligence
