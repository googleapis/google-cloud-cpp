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

#include "docfx/doxygen2toc.h"
#include "docfx/doxygen_pages.h"
#include "docfx/parse_arguments.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  auto config = docfx::ParseArguments({argv, argv + argc});
  pugi::xml_document doc;
  auto load = doc.load_file(config.input_filename.c_str());
  if (!load) throw std::runtime_error("Error loading XML input file");

  std::ofstream("toc.yml") << docfx::Doxygen2Toc(config, doc);
  for (auto const& i : doc.select_nodes("//*[@kind='page']")) {
    auto const& page = i.node();
    auto const filename = std::string{page.attribute("id").as_string()} + ".md";
    std::ofstream(filename) << docfx::Page2Markdown(config, page);
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception thrown: " << ex.what() << "\n";
  return 1;
}
