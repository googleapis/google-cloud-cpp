// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OVERRIDE_DEFAULT_PROJECT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OVERRIDE_DEFAULT_PROJECT_H_

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Override the default project.
 *
 * In a small number of operations it is convenient to override the project id
 * configured in a `storage::Client`. This option overrides the project id,
 * without requiring additional overloads for each function.
 */
struct OverrideDefaultProject
    : public internal::ComplexOption<OverrideDefaultProject, std::string> {
  using ComplexOption<OverrideDefaultProject, std::string>::ComplexOption;
  static char const* name() { return "override_default_project"; }
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OVERRIDE_DEFAULT_PROJECT_H_
