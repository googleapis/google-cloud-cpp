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

#include "google/cloud/texttospeech/text_to_speech_client.h"
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

  namespace texttospeech = ::google::cloud::texttospeech;
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
