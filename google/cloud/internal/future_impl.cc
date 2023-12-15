// Copyright 2018 Google LLC
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

#include "google/cloud/internal/future_impl.h"
#include "google/cloud/terminate_handler.h"
#include <stdexcept>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

[[noreturn]] void ThrowFutureError(std::future_errc ec, char const* msg) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  (void)msg;  // disable unused argument warning.
  throw std::future_error(ec);
#else
  std::string full_msg = "future_error[";
  full_msg += std::make_error_code(ec).message();
  full_msg += "]: ";
  full_msg += msg;
  google::cloud::Terminate(full_msg.c_str());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

[[noreturn]] void ThrowDelegate(std::exception_ptr ex, char const* msg) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  (void)msg;  // disable unused argument warning.
  std::rethrow_exception(std::move(ex));
#else
  (void)ex;
  google::cloud::Terminate(msg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

std::exception_ptr MakeFutureError(std::future_errc ec) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  return std::make_exception_ptr(std::future_error(ec));
#else
  (void)ec;
  // We cannot create a valid `std::exception_ptr` in this case. It does not
  // matter too: the application can only get to the exception via `.get()`,
  // which terminates the application via `ThrowDelegate()`.
  return nullptr;
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
