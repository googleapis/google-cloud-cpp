// Copyright 2020 Google LLC
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
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H

#include "generator/internal/class_generator_interface.h"
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Extracts service wide substitution data required by all class generators from
 * the provided descriptor.
 */
std::map<std::string, std::string> CreateServiceVars(
    google::protobuf::ServiceDescriptor const& descriptor,
    std::vector<std::pair<std::string, std::string>> const& initial_values);

/**
 * Extracts method specific substitution data for each method in the service.
 */
std::map<std::string, std::map<std::string, std::string>> CreateMethodVars(
    google::protobuf::ServiceDescriptor const& service);

/**
 * Creates and initializes the collection of ClassGenerators necessary to
 * generate all code for the given service.
 */
std::vector<std::unique_ptr<ClassGeneratorInterface>> MakeGenerators(
    google::protobuf::ServiceDescriptor const* service,
    google::protobuf::compiler::GeneratorContext* context,
    std::vector<std::pair<std::string, std::string>> const& vars);

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_DESCRIPTOR_UTILS_H
