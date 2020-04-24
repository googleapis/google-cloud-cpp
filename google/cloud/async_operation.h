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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_OPERATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_OPERATION_H

#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * The result of an async timer operation.
 *
 * Callbacks for async timers will receive an object of this class.
 */
struct AsyncTimerResult {
  std::chrono::system_clock::time_point deadline;
  bool cancelled;
};

/**
 * Represents a pending asynchronous operation.
 *
 * It can either be a simple RPC, or a more complex operation involving
 * potentially many RPCs, sleeping and processing.
 */
class AsyncOperation {
 public:
  virtual ~AsyncOperation() = default;

  /**
   * Requests that the operation be canceled.
   */
  virtual void Cancel() = 0;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_OPERATION_H
