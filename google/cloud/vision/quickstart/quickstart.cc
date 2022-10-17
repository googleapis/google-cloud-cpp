// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/vision/image_annotator_client.h"
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

  namespace vision = ::google::cloud::vision;
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
