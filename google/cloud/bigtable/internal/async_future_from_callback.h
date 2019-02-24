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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_FUTURE_FROM_CALLBACK_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_FUTURE_FROM_CALLBACK_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/future.h"
#include <google/protobuf/empty.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * A callback for Async* operations that stores the result in a `future`.
 *
 * The `noex` layer of the Bigtable client uses callbacks to communicate the
 * result of an operation to the application. For the layer with exceptions we
 * use `google::cloud::future<>` which offers a more powerful API, as the
 * application can chose to use callbacks or block until the future completes.
 *
 * @note With C++14 this class would be unnecessary, as a extended lambda
 * capture can do the work, but we need to support C++11, so an ad-hoc class is
 * needed.
 *
 * @tparam R the result type for the callback.
 */
template <typename R>
class AsyncFutureFromCallback {
 public:
  explicit AsyncFutureFromCallback(promise<R> p, char const* w)
      : promise_(std::move(p)), where_(w) {}

  void operator()(CompletionQueue& cq, R& result, grpc::Status& status) {
    if (!status.ok()) {
      promise_.set_exception(
          std::make_exception_ptr(GRpcError(where_, status)));
      return;
    }
    promise_.set_value(std::move(result));
  }

 private:
  promise<R> promise_;
  char const* where_;
};

/**
 * Specialize `AsyncFutureFromCallback` for `void` results.
 *
 * When the asynchronous operation returns `void` (or the equivalent
 * `google::protobuf::Empty`), we should use `future<void>` to represent the
 * result, but the API for these futures is slightly different. This
 * specialization deals with those differences.
 */
template <>
class AsyncFutureFromCallback<void> {
 public:
  explicit AsyncFutureFromCallback(promise<void> p, char const* w)
      : promise_(std::move(p)), where_(w) {}

  void operator()(CompletionQueue& cq, google::protobuf::Empty& result,
                  grpc::Status& status) {
    if (!status.ok()) {
      promise_.set_exception(
          std::make_exception_ptr(GRpcError(where_, status)));
      return;
    }
    promise_.set_value();
  }

  promise<void> promise_;
  char const* where_;
};

/**
 * Creates a `AsyncFutureFromCallback` of the correct type.
 *
 * Given a `promise<T>` deduce the desired type of `AsyncFutureFromCallback<T>`
 * and returns a new instance.
 */
template <typename T>
AsyncFutureFromCallback<T> MakeAsyncFutureFromCallback(promise<T> p,
                                                       const char* w) {
  return AsyncFutureFromCallback<T>(std::move(p), w);
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_FUTURE_FROM_CALLBACK_H_
