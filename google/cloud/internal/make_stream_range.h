// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STREAM_RANGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STREAM_RANGE_H

#include "google/cloud/internal/call_context.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>
#include <utility>
#include <variant>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename T>
using StreamReaderExplicit =
    std::function<std::variant<Status, T>(Options const&)>;

template <typename T>
class StreamReaderWrapper {
 public:
  explicit StreamReaderWrapper(ImmutableOptions options,
                               StreamReaderExplicit<T> reader)
      : call_context_(std::move(options)), reader_(std::move(reader)) {}

  ~StreamReaderWrapper() {
    ScopedCallContext scope(call_context_);
    reader_ = nullptr;
  }

  StreamReaderWrapper(StreamReaderWrapper const&) = delete;
  StreamReaderWrapper& operator=(StreamReaderWrapper const&) = delete;
  StreamReaderWrapper(StreamReaderWrapper&&) = delete;
  StreamReaderWrapper& operator=(StreamReaderWrapper&&) = delete;

  std::variant<Status, T> operator()() {
    ScopedCallContext scope(call_context_);
    return reader_(*call_context_.options);
  }

 private:
  CallContext call_context_;
  StreamReaderExplicit<T> reader_;
};

/**
 * Factory to construct a `StreamRange<T>` with the given `StreamReader<T>`.
 *
 * Callers should explicitly specify the `T` parameter when calling this
 * function template so that lambdas will implicitly convert to the underlying
 * `StreamReader<T>` (i.e., std::function). For example:
 *
 * @code
 * StreamRange<int> empty = MakeStreamRange<int>([] { return Status{}; });
 * @endcode
 */
template <typename T>
StreamRange<T> MakeStreamRange(ImmutableOptions options,
                               StreamReaderExplicit<T> reader) {
  auto impl = std::make_shared<StreamReaderWrapper<T>>(std::move(options),
                                                       std::move(reader));
  return MakeStreamRangeImpl<T>([impl = std::move(impl)] { return (*impl)(); });
}

// TODO(#12359) - remove overload when `*Stub` consumes Options.
template <typename T>
StreamRange<T> MakeStreamRange(StreamReader<T> reader) {
  auto wrapper = [r = std::move(reader)](Options const&) { return r(); };
  return MakeStreamRange<T>(SaveCurrentOptions(), std::move(wrapper));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MAKE_STREAM_RANGE_H
