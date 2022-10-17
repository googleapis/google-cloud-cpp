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

#include "google/cloud/speech/speech_client.h"
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

  namespace speech = ::google::cloud::speech;
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
