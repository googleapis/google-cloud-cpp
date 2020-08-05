// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_VERSION_H
#define GOOGLE_CLOUD_CPP_GENERATOR_VERSION_H

#include "generator/version.h"
#include "generator/version_info.h"
#include <string>

#define GOOGLE_CLOUD_CPP_GENERATOR_NS                              \
  GOOGLE_CLOUD_CPP_VEVAL(GOOGLE_CLOUD_CPP_GENERATOR_VERSION_MAJOR, \
                         GOOGLE_CLOUD_CPP_GENERATOR_VERSION_MINOR)

namespace google {
/**
 * The namespace Google Cloud Platform C++ microgenerator.
 */
namespace cxx_microgenerator {
/**
 * The inlined, versioned namespace for the Cloud C++ client microgenerator
 */
inline namespace GOOGLE_CLOUD_CPP_GENERATOR_NS {
}  // namespace GOOGLE_CLOUD_CPP_GENERATOR_NS
}  // namespace cxx_microgenerator
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_VERSION_H
