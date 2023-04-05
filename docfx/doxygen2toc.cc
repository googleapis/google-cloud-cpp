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
#include <yaml-cpp/yaml.h>
#include <sstream>

namespace docfx {

std::string Doxygen2Toc(Config const& config, pugi::xml_document const& doc) {
  auto const uid = std::string{"cloud.google.com/cpp/"} + config.library;
  YAML::Emitter out;
  out << YAML::BeginMap                                        //
      << YAML::Key << "name" << YAML::Value << config.library  //
      << YAML::Key << "items" << YAML::Value                   //
      << YAML::BeginSeq;
  for (auto const& [filename, name] : PagesToc(doc)) {
    out << YAML::BeginMap                                  //
        << YAML::Key << "name" << YAML::Value << name      //
        << YAML::Key << "href" << YAML::Value << filename  //
        << YAML::EndMap;
  }
  for (auto const& [filename, name] : GroupsToc(doc)) {
    out << YAML::BeginMap                              //
        << YAML::Key << "uid" << YAML::Value << name   //
        << YAML::Key << "name" << YAML::Value << name  //
        << YAML::EndMap;
  }
  for (auto const& [filename, name] : CompoundToc(doc)) {
    out << YAML::BeginMap                              //
        << YAML::Key << "uid" << YAML::Value << name   //
        << YAML::Key << "name" << YAML::Value << name  //
        << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  std::ostringstream os;
  os << "### YamlMime:TableOfContent\n" << out.c_str() << "\n";
  return std::move(os).str();
}

}  // namespace docfx
