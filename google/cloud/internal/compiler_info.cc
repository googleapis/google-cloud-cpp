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
#include <sstream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * The macros for determining the compiler ID are taken from:
 * https://gitlab.kitware.com/cmake/cmake/tree/v3.5.0/Modules/Compiler/\*-DetermineCompiler.cmake
 * We do not care to detect every single compiler possible and only target the
 * most popular ones.
 *
 * Order is significant as some compilers can define the same macros.
 */
std::string CompilerId() {
#if defined(__apple_build_version__) && defined(__clang__)
  return "AppleClang";
#elif defined(__clang__)
  return "Clang";
#elif defined(__GNUC__)
  return "GNU";
#elif defined(_MSC_VER)
  return "MSVC";
#endif

  return "Unknown";
}

std::string CompilerVersion() {
  std::ostringstream os;

#if defined(__apple_build_version__) && defined(__clang__)
  os << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__
     << "." << __apple_build_version__;
  return std::move(os).str();
#elif defined(__clang__)
  os << __clang_major__ << "." << __clang_minor__ << "."
     << __clang_patchlevel__;
  return std::move(os).str();
#elif defined(__GNUC__)
  os << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
  return std::move(os).str();
#elif defined(_MSC_VER)
  os << _MSC_VER / 100 << ".";
  os << _MSC_VER % 100;
#if defined(_MSC_FULL_VER)
#if _MSC_VER >= 1400
  os << "." << _MSC_FULL_VER % 100000;
#else
  os << "." << _MSC_FULL_VER % 10000;
#endif
#endif
  return std::move(os).str();
#endif

  return "Unknown";
}

std::string CompilerFeatures() {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  return "ex";
#else
  return "noex";
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

std::string LanguageVersion() {
  auto constexpr kMagicVersionCxx98 = 199711L;
  auto constexpr kMagicVersionCxx11 = 201103L;
  auto constexpr kMagicVersionCxx14 = 201402L;
  auto constexpr kMagicVersionCxx17 = 201703L;
  switch (__cplusplus) {
    case kMagicVersionCxx98:
      return "1998";
    case kMagicVersionCxx11:
      return "2011";
    case kMagicVersionCxx14:
      return "2014";
    case kMagicVersionCxx17:
      return "2017";
    default:
      return "unknown";
  }
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
