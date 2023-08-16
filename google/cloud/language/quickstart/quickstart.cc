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
#include "google/cloud/language/v2/language_client.h"
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

  namespace language = ::google::cloud::language_v2;
  auto client = language::LanguageServiceClient(
      language::MakeLanguageServiceConnection());

  google::cloud::language::v2::Document document;
  document.set_type(google::cloud::language::v2::Document::PLAIN_TEXT);
  document.set_content(kText);
  document.set_language_code("en-US");

  auto response = client.AnalyzeEntities(document);
  if (!response) throw std::move(response).status();

  for (auto const& entity : response->entities()) {
    if (entity.type() != google::cloud::language::v2::Entity::NUMBER) continue;
    std::cout << entity.DebugString() << "\n";
  }

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
