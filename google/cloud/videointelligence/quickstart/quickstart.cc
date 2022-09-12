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

#include "google/cloud/videointelligence/video_intelligence_client.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <stdexcept>

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

  namespace videointelligence = ::google::cloud::videointelligence;
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
  if (!response) throw response.status();

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
  std::cerr << "google::cloud::Status raised: " << status << "\n";
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
