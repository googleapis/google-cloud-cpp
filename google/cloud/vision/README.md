# Cloud Vision API C++ Client Library

This directory contains an idiomatic C++ client library for the
[Cloud Vision API][cloud-service-docs]. Integrate Google Vision features,
including image labeling, face, logo, and landmark detection, optical character
recognition (OCR), and detection of explicit content, into applications.

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
#include "google/cloud/vision/v1/image_annotator_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  auto constexpr kDefaultUri =
      "gs://cloud-samples-data/vision/label/wakeupcat.jpg";
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0] << " [gcs-uri]\n"
              << "  The gcs-uri must be in gs://... format. It defaults to "
              << kDefaultUri << "\n";
    return 1;
  }
  auto uri = std::string{argc == 2 ? argv[1] : kDefaultUri};

  namespace vision = ::google::cloud::vision_v1;
  auto client =
      vision::ImageAnnotatorClient(vision::MakeImageAnnotatorConnection());

  // Define the image we want to annotate
  google::cloud::vision::v1::Image image;
  image.mutable_source()->set_image_uri(uri);
  // Create a request to annotate this image with Request text annotations for a
  // file stored in GCS.
  google::cloud::vision::v1::AnnotateImageRequest request;
  *request.mutable_image() = std::move(image);
  request.add_features()->set_type(
      google::cloud::vision::v1::Feature::TEXT_DETECTION);

  google::cloud::vision::v1::BatchAnnotateImagesRequest batch_request;
  *batch_request.add_requests() = std::move(request);
  auto batch = client.BatchAnnotateImages(batch_request);
  if (!batch) throw std::move(batch).status();

  // Find the longest annotation and print it
  auto result = std::string{};
  for (auto const& response : batch->responses()) {
    for (auto const& annotation : response.text_annotations()) {
      if (result.size() < annotation.description().size()) {
        result = annotation.description();
      }
    }
  }
  std::cout << "The image contains this text: " << result << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
```

<!-- inject-quickstart-end -->

## More Information

- Official documentation about the [Cloud Vision API][cloud-service-docs]
  service
- [Reference doxygen documentation][doxygen-link] for each release of this
  client library
- Detailed header comments in our [public `.h`][source-link] files

[cloud-service-docs]: https://cloud.google.com/vision
[doxygen-link]: https://cloud.google.com/cpp/docs/reference/vision/latest/
[source-link]: https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/vision
