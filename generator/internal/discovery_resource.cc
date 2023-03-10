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

#include "generator/internal/discovery_resource.h"
#include "generator/internal/codegen_utils.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/algorithm.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include <iostream>

namespace google {
namespace cloud {
namespace generator_internal {

DiscoveryResource::DiscoveryResource(std::string name, std::string default_host,
                                     std::string base_path, nlohmann::json json)
    : name_(std::move(name)),
      default_host_(std::move(default_host)),
      base_path_(std::move(base_path)),
      json_(std::move(json)) {}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
