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

#include "google/cloud/translate/translation_client.h"
#include "google/cloud/project.h"
#include <iostream>

auto constexpr kText = R"""(
Four score and seven years ago our fathers brought forth on this
continent, a new nation, conceived in Liberty, and dedicated to
the proposition that all men are created equal.)""";

int main(int argc, char* argv[]) try {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " project-id "
              << "[target-language (default: es-419)]\n";
    return 1;
  }

  namespace translate = ::google::cloud::translate;
  auto client = translate::TranslationServiceClient(
      translate::MakeTranslationServiceConnection());

  auto const project = google::cloud::Project(argv[1]);
  auto const target = std::string{argc >= 3 ? argv[2] : "es-419"};
  auto response =
      client.TranslateText(project.FullName(), target, {std::string{kText}});
  if (!response) throw std::move(response).status();

  for (auto const& translation : response->translations()) {
    std::cout << translation.translated_text() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
