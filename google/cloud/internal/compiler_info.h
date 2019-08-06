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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPILER_INFO_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPILER_INFO_H_

#include "google/cloud/version.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * Returns the compiler ID.
 *
 * The Compiler ID is a string like "GNU" or "Clang", as described by
 * https://cmake.org/cmake/help/v3.5/variable/CMAKE_LANG_COMPILER_ID.html
 */
std::string CompilerId();

/**
 * Returns the compiler version.
 *
 * This string will be something like "9.1.1".
 */
std::string CompilerVersion();

/**
 * Returns certain interesting compiler features.
 *
 * Currently this returns one of "ex" or "noex" to indicate whether or not
 * C++ exceptions are enabled.
 */
std::string CompilerFeatures();

/**
 * Returns the 4-digit year of the C++ language standard.
 */
std::string LanguageVersion();

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPILER_INFO_H_
