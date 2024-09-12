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

#include "google/cloud/generativelanguage/v1/generative_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/example_driver.h"
#include <string>
#include <vector>

namespace {

void GeminiTextGenTextOnlyPrompt(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw google::cloud::testing_util::Usage(
        "gemini-text-gen-text-only-prompt <model-name> [<content>]+");
  }
  // [START text_gen_text_only_prompt]
  namespace gemini = ::google::cloud::generativelanguage_v1;
  namespace gemini_proto = ::google::ai::generativelanguage::v1;
  [](std::string const& model, std::vector<std::string> const& content) {
    gemini::GenerativeServiceClient client(
        gemini::MakeGenerativeServiceConnection());
    std::vector<gemini_proto::Content> contents;
    for (auto const& c : content) {
      gemini_proto::Content content;
      content.add_parts()->set_text(c);
      contents.push_back(std::move(content));
    }

    auto response = client.GenerateContent(model, contents);
    if (!response) throw std::move(response).status();

    for (auto const& candidate : response->candidates()) {
      for (auto const& p : candidate.content().parts()) {
        std::cout << p.text() << "\n";
      }
    }
  }
  // [END text_gen_text_only_prompt]
  (argv.at(0), {argv.begin() + 1, argv.end()});
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_QUOTA_PROJECT",
  });

  GeminiTextGenTextOnlyPrompt(
      {"models/gemini-1.5-flash", "Write a story about a magic backpack."});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example(
      {{"gemini-text-gen-text-only-prompt", GeminiTextGenTextOnlyPrompt},
       {"auto", AutoRun}});
  return example.Run(argc, argv);
}
