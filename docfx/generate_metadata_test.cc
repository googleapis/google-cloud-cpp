// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "docfx/generate_metadata.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace docfx {
namespace {

TEST(GenerateMetadata, Basic) {
  auto const config = Config{"test-only-input-filename", "test-only-library",
                             "test-only-version"};
  auto const generated = GenerateMetadata(config);
  auto const actual = nlohmann::json::parse(generated);
  auto const expected = nlohmann::json{
      {"language", "cpp"},
      {"name", "test-only-library"},
      {"version", "test-only-version"},
      {"xrefs", {"devsite://cpp/common"}},
  };
  EXPECT_EQ(expected, actual);
}

TEST(GenerateMetadata, Common) {
  auto const config =
      Config{"test-only-input-filename", "cloud", "test-only-version"};
  auto const generated = GenerateMetadata(config);
  auto const actual = nlohmann::json::parse(generated);
  auto const expected = nlohmann::json{
      {"language", "cpp"},
      {"name", "common"},
      {"version", "test-only-version"},
  };
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace docfx
