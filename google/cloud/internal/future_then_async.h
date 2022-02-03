// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_ASYNC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_ASYNC_H
/**
 * @file
 *
 * Define the `then_async` template function.
 *
 * This function allows for generic asynchronous continuations using
 * google::cloud::future. In particular, it allows chaining of async
 * operations without increasing the nesting level.
 * 
 * Usage of this utility would look like the below, which is a series of
 * asynchronous operations written in an almost linear style:
 * 
 * future<std::string> f1 = ...
 * future<int> f2 = then_async(std::move(f1), [](std::string value) -> future<int> {
 *     return StartAsyncOp(std::move(value));
 * });
 * future<Status> f3 = then_async(std::move(f2), [](int value) -> future<Status> {
 *     return FinishAsyncOp(value);
 * });
 * return f3;
 */

#include "google/cloud/future.h"
#include "google/cloud/version.h"
#include "google/cloud/internal/invoke_result.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <class FutureT>
struct future_value_type;
template <class T>
struct future_value_type<future<T>> {
    using type = T;
};
template <class T>
using future_value_type_t = typename future_value_type<T>::type;

void link_future_and_promise(future<void> fut, promise<void> p) {
    fut.then([p]{ p.set_value(); });
}

template <class T>
void link_future_and_promise(future<T> fut, promise<T> p) {
    fut.then([p](T val){ p.set_value(std::move(val)); });
}

template <class Functor, class ReturnT = invoke_result_t<Functor>>
ReturnT then_async(future<void> fut, Functor&& f) {
    promise<future_value_type_t<ReturnT>> to_return;
    fut.then(absl::bind_front([to_return](Functor&& f) {
        link_future_and_promise(std::forward<Functor>(f)(), to_return);
    }, std::forward<Functor>(f)));
    return to_return.get_future();
}

template <class T, class Functor, class ReturnT = invoke_result_t<Functor, T>>
ReturnT then_async(future<T> fut, Functor&& f) {
    promise<future_value_type_t<ReturnT>> to_return;
    fut.then(absl::bind_front([to_return](Functor&& f, T val) {
        link_future_and_promise(std::forward<Functor>(f)(std::move(val)), to_return);
    }, std::forward<Functor>(f)));
    return to_return.get_future();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_THEN_ASYNC_H
