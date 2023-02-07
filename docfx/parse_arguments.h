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

#ifndef GOOGLE_CLOUD_CPP_DOCFX_PARSE_ARGUMENTS_H
#define GOOGLE_CLOUD_CPP_DOCFX_PARSE_ARGUMENTS_H

#include <string>
#include <vector>

struct Config {
  std::string input_filename;
  std::string library;
};

Config ParseArguments(std::vector<std::string> const& args);

#endif  // GOOGLE_CLOUD_CPP_DOCFX_PARSE_ARGUMENTS_H
