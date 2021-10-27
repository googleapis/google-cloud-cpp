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

#include "google/cloud/internal/compiler_info.h"
#include "google/cloud/internal/port_platform.h"
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string CombineVersions(int major, int minor, int patch,
                            std::string const& build_id) {
  std::ostringstream os;
  os << major << '.' << minor;
  if (patch != 0) os << '.' << patch;
  if (!build_id.empty()) os << '+' << build_id;
  return std::move(os).str();
}

std::string LanguageVersion(int language_version) {
  auto constexpr kMagicVersionCxx98 = 199711L;
  auto constexpr kMagicVersionCxx11 = 201103L;
  auto constexpr kMagicVersionCxx14 = 201402L;
  auto constexpr kMagicVersionCxx17 = 201703L;
  auto constexpr kMagicVersionCxx20 = 202002L;
  switch (language_version) {
    case kMagicVersionCxx98:
      return "1998";
    case kMagicVersionCxx11:
      return "2011";
    case kMagicVersionCxx14:
      return "2014";
    case kMagicVersionCxx17:
      return "2017";
    case kMagicVersionCxx20:
      return "2020";
    default:
      return "unknown";
  }
}

/**
 * The macros for determining the compiler ID are taken from:
 * https://gitlab.kitware.com/cmake/cmake/tree/v3.5.0/Modules/Compiler/\*-DetermineCompiler.cmake
 * We do not care to detect every single compiler possible and only target the
 * most popular ones.
 *
 * Order is significant as some compilers can define the same macros.
 */
std::string CompilerId() { return ApplicationCompilerId(); }

std::string CompilerVersion() { return ApplicationCompilerVersion(); }

std::string CompilerFeatures() { return ApplicationCompilerFeatures(); }

std::string LanguageVersion() { return ApplicationLanguageVersion(); }

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
