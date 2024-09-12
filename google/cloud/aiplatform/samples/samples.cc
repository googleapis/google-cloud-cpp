// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/aiplatform/v1/prediction_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/location.h"
#include "google/cloud/testing_util/example_driver.h"
#include <string>
#include <vector>

namespace {

void GeminiGenerateFromTextInput(std::vector<std::string> const& argv) {
  if (argv.size() < 4) {
    throw google::cloud::testing_util::Usage(
        "gemini-generate-from-text-input <project> <location> <model-name> "
        "[<content>]+");
  }
  // [START generativeaionvertexai_gemini_generate_from_text_input]
  namespace vertex_ai = ::google::cloud::aiplatform_v1;
  namespace vertex_ai_proto = ::google::cloud::aiplatform::v1;
  [](std::string const& project_id, std::string const& location_id,
     std::string const& model, std::vector<std::string> const& content) {
    google::cloud::Location location(project_id, location_id);
    auto client = vertex_ai::PredictionServiceClient(
        vertex_ai::MakePredictionServiceConnection(location.location_id()));

    std::vector<vertex_ai_proto::Content> contents;
    for (auto const& c : content) {
      vertex_ai_proto::Content content;
      content.set_role("user");
      content.add_parts()->set_text(c);
      contents.push_back(std::move(content));
    }
    auto response = client.GenerateContent(
        location.FullName() + "/publishers/google/models/" + model, contents);
    if (!response) throw std::move(response).status();

    for (auto const& candidate : response->candidates()) {
      for (auto const& p : candidate.content().parts()) {
        std::cout << p.text() << "\n";
      }
    }
  }
  // [END generativeaionvertexai_gemini_generate_from_text_input]
  (argv.at(0), argv.at(1), argv.at(2), {argv.begin() + 3, argv.end()});
}

void GeminiGenerateWithImage(std::vector<std::string> const& argv) {
  if (argv.size() != 6) {
    throw google::cloud::testing_util::Usage(
        "gemini-generate-from-text-input <project> <location> <model-name> "
        "<prompt> <mime-type> <file-uri>");
  }
  // [START generativeaionvertexai_gemini_get_started]
  namespace vertex_ai = ::google::cloud::aiplatform_v1;
  namespace vertex_ai_proto = ::google::cloud::aiplatform::v1;
  [](std::string const& project_id, std::string const& location_id,
     std::string const& model, std::string const& prompt,
     std::string const& mime_type, std::string const& file_uri) {
    google::cloud::Location location(project_id, location_id);
    auto client = vertex_ai::PredictionServiceClient(
        vertex_ai::MakePredictionServiceConnection(location.location_id()));

    vertex_ai_proto::GenerateContentRequest request;
    request.set_model(location.FullName() + "/publishers/google/models/" +
                      model);
    auto generation_config = request.mutable_generation_config();
    generation_config->set_temperature(0.4);
    generation_config->set_top_k(32);
    generation_config->set_top_p(1);
    generation_config->set_max_output_tokens(2048);

    auto contents = request.add_contents();
    contents->set_role("user");
    contents->add_parts()->set_text(prompt);
    auto image_part = contents->add_parts();
    image_part->mutable_file_data()->set_file_uri(file_uri);
    image_part->mutable_file_data()->set_mime_type(mime_type);

    auto response = client.GenerateContent(request);
    if (!response) throw std::move(response).status();

    for (auto const& candidate : response->candidates()) {
      for (auto const& p : candidate.content().parts()) {
        std::cout << p.text() << "\n";
      }
    }
  }
  // [END generativeaionvertexai_gemini_get_started]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3), argv.at(4), argv.at(5));
}

void GeminiVideoWithAudio(std::vector<std::string> const& argv) {
  if (argv.size() != 6) {
    throw google::cloud::testing_util::Usage(
        "gemini-generate-from-text-input <project> <location> <model-name> "
        "<prompt> <mime-type> <file-uri>");
  }
  // [START generativeaionvertexai_gemini_video_with_audio]
  namespace vertex_ai = ::google::cloud::aiplatform_v1;
  namespace vertex_ai_proto = ::google::cloud::aiplatform::v1;
  [](std::string const& project_id, std::string const& location_id,
     std::string const& model, std::string const& prompt,
     std::string const& mime_type, std::string const& file_uri) {
    google::cloud::Location location(project_id, location_id);
    auto client = vertex_ai::PredictionServiceClient(
        vertex_ai::MakePredictionServiceConnection(location.location_id()));

    vertex_ai_proto::GenerateContentRequest request;
    request.set_model(location.FullName() + "/publishers/google/models/" +
                      model);
    auto contents = request.add_contents();
    contents->set_role("user");
    contents->add_parts()->set_text(prompt);
    auto image_part = contents->add_parts();
    image_part->mutable_file_data()->set_file_uri(file_uri);
    image_part->mutable_file_data()->set_mime_type(mime_type);

    auto response = client.GenerateContent(request);
    if (!response) throw std::move(response).status();

    for (auto const& candidate : response->candidates()) {
      for (auto const& p : candidate.content().parts()) {
        std::cout << p.text() << "\n";
      }
    }
  }
  // [END generativeaionvertexai_gemini_video_with_audio]
  (argv.at(0), argv.at(1), argv.at(2), argv.at(3), argv.at(4), argv.at(5));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  std::cout << "Executing GeminiGenerateFromTextInput sample:\n";
  GeminiGenerateFromTextInput(
      {project_id, "us-central1", "gemini-1.5-flash-001",
       "What's a good name for a flower shop that specializes in selling "
       "bouquets of dried flowers?"});

  std::cout << "\nExecuting GeminiGenerateWithImage sample:\n";
  GeminiGenerateWithImage({project_id, "us-central1", "gemini-1.5-flash-001",
                           "What's in this photo?", "image/png",
                           "gs://generativeai-downloads/images/scones.jpg"});

  std::cout << "\nExecuting GeminiVideoWithAudio sample:\n";
  GeminiVideoWithAudio(
      {project_id, "us-central1", "gemini-1.5-flash-001",
       "Provide a description of the video.\n"
       "The description should also contain anything important which people "
       "say in the video.",
       "video/mp4", "gs://cloud-samples-data/generative-ai/video/pixel8.mp4"});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example(
      {{"gemini-generate-with-image", GeminiGenerateWithImage},
       {"gemini-generate-from-text-input", GeminiGenerateFromTextInput},
       {"gemini-video-with-audio", GeminiVideoWithAudio},
       {"auto", AutoRun}});
  return example.Run(argc, argv);
}
