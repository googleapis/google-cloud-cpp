// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PORT_PLATFORM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PORT_PLATFORM_H

/**
 * @file
 *
 * Platform portability details.
 *
 * This file discover details about the platform (compiler, OS, and hardware
 * features) and defines objects and types (mostly macros) to adapt the code.
 * The file should be fairly minimal because we rely on gRPC to deal with most
 * of the platform problems.
 */

// Turn off clang-format because these nested #if/#endif blocks are more
// readable with indentation.
// clang-format off

// Microsoft Visual Studio does not define __cplusplus correctly by default:
//
//   https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus
//
// Here we define a macro to hide this detail.
//
// We considered using the `/Zc:__cplusplus` flag with MSVC, but because we
// use `__cplusplus` in headers we would be forcing downstream code to set
// this flag too. That might break existing user code that wants to use our
// library, and we prefer not to do so.
#ifdef _MSC_VER
#  define GOOGLE_CLOUD_CPP_CPP_VERSION _MSVC_LANG
#else
#  define GOOGLE_CLOUD_CPP_CPP_VERSION __cplusplus
#endif  // _MSC_VER

// Abort compilation if the compiler does not support C++11.
#if GOOGLE_CLOUD_CPP_CPP_VERSION < 201103L
#  error "C++11 or newer is required"
#endif  // GOOGLE_CLOUD_CPP_CPP_VERSION < 201103L

// Abort the build if the version of the compiler is too old. With CMake we
// never start the build, but with Bazel we may start the build only to find
// that the compiler is too old. This also simplifies some of the testing
// further down in this file. Because Clang defines both __GNUC__ and __clang__
// test for the Clang version first (sigh).
#if defined(__clang__)
#  if __clang_major__ < 6
#    error "Only Clang >= 6.0 is supported."
#  endif  // Clang < 6.0
#elif defined(__GNUC__)
#  if __GNUC__ < 5 || (__GNUC__ == 5 && __GNUC_MINOR__ < 4)
#    error "Only GCC >= 5.4 is supported."
#  endif  // GCC < 5.4
#endif  // defined(__clang__)

// Discover if exceptions are enabled and define them as needed.
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#  error "GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS should not be set directly."
#elif defined(__clang__)
#  if defined(__EXCEPTIONS) && __has_feature(cxx_exceptions)
#    define GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS 1
#  endif  // defined(__EXCEPTIONS) && __has_feature(cxx_exceptions)
#elif defined(_MSC_VER)
#  if defined(_CPPUNWIND)
#    define GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS 1
#  endif  // defined(_CPPUNWIND)
#elif defined(__GNUC__)
#  if (__GNUC__ < 5) && defined(__EXCEPTIONS)
#    define GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS 1
#  elif (__GNUC__ >= 5) && defined(__cpp_exceptions)
#    define GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS 1
#  endif  // (__GNUC__ >= 5) && defined(__cpp_exceptions)
#elif defined(__cpp_exceptions)
   // This should work in increasingly more and more compilers.
   // https://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations
#  define GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS 1
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

// clang-format on

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_PORT_PLATFORM_H
