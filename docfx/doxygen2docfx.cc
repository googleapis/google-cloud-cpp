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
#include "docfx/doxygen2yaml.h"
#include "docfx/doxygen_groups.h"
#include "docfx/doxygen_pages.h"
#include "docfx/generate_metadata.h"
#include "docfx/parse_arguments.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  auto config = docfx::ParseArguments({argv, argv + argc});
  pugi::xml_document doc;
  auto load = doc.load_file(config.input_filename.c_str());
  if (!load) throw std::runtime_error("Error loading XML input file");

  std::ofstream("docs.metadata.json") << docfx::GenerateMetadata(config);

  std::ofstream("toc.yml") << docfx::Doxygen2Toc(config, doc);
  for (auto const& [filename, name] : docfx::PagesToc(doc)) {
    auto const filter = std::string{"//compounddef[@id='"} + name + "']";
    auto const node = doc.select_node(filter.c_str()).node();
    std::ofstream(filename) << docfx::Page2Markdown(node);
  }

  for (auto const& [filename, name] : docfx::GroupsToc(doc)) {
    auto const filter = std::string{"//compounddef[@id='"} + name + "']";
    auto const node = doc.select_node(filter.c_str()).node();
    std::ofstream(filename) << docfx::Group2Yaml(node);
  }

  for (auto const& [filename, name] : docfx::CompoundToc(doc)) {
    auto const filter = std::string{"//compounddef[@id='"} + name + "']";
    auto const node = doc.select_node(filter.c_str()).node();
    std::ofstream(filename) << docfx::Compound2Yaml(node);
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception thrown: " << ex.what() << "\n";
  return 1;
}
