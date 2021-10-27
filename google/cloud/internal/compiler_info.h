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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPILER_INFO_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPILER_INFO_H

#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
std::string CombineVersions(int major, int minor, int patch,
                            std::string const& build_id = {});
std::string LanguageVersion(int language_version);

// Note the use of an anonymous namespace to avoid ODR violations.
namespace {  // NOLINT(google-build-namespaces)

inline std::string ApplicationCompilerId() {
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

inline std::string ApplicationCompilerVersion() {
#if defined(__apple_build_version__) && defined(__clang__)
  return CombineVersions(__clang_major__, __clang_minor__, __clang_patchlevel__,
                         std::to_string(__apple_build_version__));
#elif defined(__clang__)
  return CombineVersions(__clang_major__, __clang_minor__,
                         __clang_patchlevel__);
#elif defined(__GNUC__)
  return CombineVersions(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
#if !defined(_MSC_FULL_VER)
  auto constexpr kPatchLevel = 0;
#elif _MSC_VER >= 1400
  auto constexpr kPatchLevel = _MSC_FULL_VER % 100000;
#elif
  auto constexpr kPatchLevel = _MSC_FULL_VER % 10000;
#endif
  return CombinedVersions(_MSC_VER / 100, _MSC_VER % 100, kPatchLevel);
#else
  return "Unknown";
#endif
}

inline std::string ApplicationCompilerFeatures() {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  return "ex";
#else
  return "noex";
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

inline std::string ApplicationLanguageVersion() {
  return LanguageVersion(GOOGLE_CLOUD_CPP_CPP_VERSION);
}

}  // namespace

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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_COMPILER_INFO_H
