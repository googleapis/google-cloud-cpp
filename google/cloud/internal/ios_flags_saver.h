// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_IOS_FLAGS_SAVER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_IOS_FLAGS_SAVER_H

#include "google/cloud/version.h"
#include <iostream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Save the formatting flags in a std::ios_base and restores them.
 *
 * This class is a helper for functions that need to restore all formatting
 * flags in a iostream. Typically it is used in the implementation of a ostream
 * operator to restore any flags changed to format the output.
 *
 * @par Example
 * @code
 * std::ostream& operator<<(std::ostream& os, MyType const& rhs) {
 *   IosFlagsSaver save_flags(io);
 *   os << "enabled=" << std::boolalpha << rhs.enabled;
 *   // more calls here ... potentially modifying more flags
 *   return os << "blah";
 * }
 * @endcode
 */
class IosFlagsSaver final {
 public:
  explicit IosFlagsSaver(std::ios_base& ios)
      : ios_(ios), flags_(ios_.flags()) {}
  ~IosFlagsSaver() { ios_.setf(flags_); }

 private:
  std::ios_base& ios_;
  std::ios_base::fmtflags flags_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_IOS_FLAGS_SAVER_H
