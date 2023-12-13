// Copyright 2013 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/future_then_impl.h"
#include <stdexcept>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

void FutureSetResultDelegate(
    absl::FunctionRef<void()> set_value,
    absl::FunctionRef<void(std::exception_ptr)> set_exception) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
    set_value();
  } catch (...) {
    // Other errors can be reported via the promise.
    set_exception(std::current_exception());
  }
#else
  (void)set_exception;
  set_value();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
