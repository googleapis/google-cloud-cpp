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

#include "docfx/parse_arguments.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace docfx {
namespace {

std::string Usage(std::string const& cmd) {
  std::ostringstream os;
  os << "Usage: " << cmd << " <infile> <library> <version>";
  return std::move(os).str();
}

}  // namespace

Config ParseArguments(std::vector<std::string> const& args) {
  if (args.empty()) throw std::runtime_error(Usage("program-name-missing"));
  if (args.size() == 2 && args[1] == "--help") {
    std::cout << Usage(args[0]) << "\n";
    std::exit(0);
  }
  if (args.size() != 4) throw std::runtime_error(Usage(args[0]));
  // It is tempting to use google::cloud::version_string(), but sometimes the
  // tool may be used to generate documentation for older versions of the
  // library.
  return Config{args[1], args[2], args[3]};
}

}  // namespace docfx
