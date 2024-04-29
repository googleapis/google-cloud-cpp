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

//! [all]
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
//! [all]
