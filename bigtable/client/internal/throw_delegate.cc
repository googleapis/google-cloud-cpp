// Copyright 2018 Google Inc.
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

#include "bigtable/client/internal/throw_delegate.h"

#include <absl/base/config.h>

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#include <stdexcept>
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

#include <iostream>

namespace {
template <typename Exception>
[[noreturn]] void RaiseException(char const* msg) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  throw Exception(msg);
#else
  std::cerr << "Aborting because exceptions are disabled: " << msg << std::endl;
  std::abort();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
}

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

[[noreturn]] void RaiseInvalidArgument(char const* msg) {
  RaiseException<std::invalid_argument>(msg);
}

[[noreturn]] void RaiseInvalidArgument(std::string const& msg) {
  RaiseException<std::invalid_argument>(msg.c_str());
}

[[noreturn]] void RaiseRangeError(char const* msg) {
  RaiseException<std::range_error>(msg);
}

[[noreturn]] void RaiseRangeError(std::string const& msg) {
  RaiseException<std::range_error>(msg.c_str());
}

[[noreturn]] void RaiseRuntimeError(char const* msg) {
  RaiseException<std::runtime_error>(msg);
}

[[noreturn]] void RaiseRuntimeError(std::string const& msg) {
  RaiseException<std::runtime_error>(msg.c_str());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
