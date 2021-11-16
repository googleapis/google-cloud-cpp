// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SCAFFOLD_GENERATOR_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SCAFFOLD_GENERATOR_H

#include "absl/types/optional.h"
#include "generator/generator_config.pb.h"
#include <iosfwd>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Returns the library short name from its path.
 *
 * In `google-cloud-cpp` libraries called `foo` live in the `google/cloud/foo`
 * directory. The names of CMake targets, Bazel rules, pkg-config modules,
 * features, etc. are based on the library name. This function returns the
 * name give a service configuration.
 *
 * This function assumes the service configuration has already been validated.
 */
std::string LibraryName(
    google::cloud::cpp::generator::ServiceConfiguration const& service);

std::map<std::string, std::string> ScaffoldVars(
    std::string const& googleapis_path, std::string const& project_root,
    google::cloud::cpp::generator::ServiceConfiguration const& service);

void GenerateScaffold(
    std::string const& googleapis_path, std::string const& output_path,
    google::cloud::cpp::generator::ServiceConfiguration const& service);

void GenerateCmakeConfigIn(std::ostream& os,
                           std::map<std::string, std::string> const& variables);
void GenerateConfigPcIn(std::ostream& os,
                        std::map<std::string, std::string> const& variables);
void GenerateReadme(std::ostream& os,
                    std::map<std::string, std::string> const& variables);
void GenerateBuild(std::ostream& os,
                   std::map<std::string, std::string> const& variables);
void GenerateCMakeLists(std::ostream& os,
                        std::map<std::string, std::string> const& variables);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_SCAFFOLD_GENERATOR_H
