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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_BUILDER_H

#include "google/cloud/internal/source_transforms.h"
#include "absl/meta/type_traits.h"
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <template <typename Source> class Accumulator>
struct MakeAccumulator {
  template <typename S, typename... A>
  Accumulator<absl::decay_t<S>> operator()(S&& s, A&&... a) {
    return Accumulator<absl::decay_t<S>>(std::forward<S>(s),
                                         std::forward<A>(a)...);
  }
};

/**
 * A Builder for objects meeting the `source<T, E>` interface.
 *
 * This class allows applications to change a `source<T>` by applying
 * transformations and accumulating results. In the future we will also
 * implement filters, sending computations to the background, and more complex
 * compositions (think "trailing average", or "reassemble chunked data").
 *
 * @tparam Source a type meeting the `source<T, E>` interface.
 */
template <typename Source>
class SourceBuilder {
 public:
  explicit SourceBuilder(Source s) : source_(std::move(s)) {}

  /// Return the contained source.
  Source&& build() && { return std::move(source_); }

  /**
   * Apply a transformation to the source returning a new builder.
   *
   * This creates a new builder containing the source transformed by @p t. This
   * new builder can apply additional changes to the source.
   */
  template <typename Transform>
  auto
  transform(Transform&& t) && -> SourceBuilder<decltype(MakeTransformedSource(
      std::declval<Source>(), std::forward<Transform>(t)))> {
    using result_source_type = decltype(MakeTransformedSource(
        std::declval<Source>(), std::forward<Transform>(t)));
    return SourceBuilder<result_source_type>(
        MakeTransformedSource(std::move(source_), std::forward<Transform>(t)));
  }

  /**
   * Apply the given accumulator type to the source.
   *
   * This applies an accumulator to the source, sending the computation to the
   * background and returning a future, satisfied when the accumulator completes
   * its work.
   */
  template <template <typename S> class Accumulator, typename... A>
  auto accumulate(A&&... a) && -> decltype(
      std::declval<
          invoke_result_t<MakeAccumulator<Accumulator>, Source, A&&...>>()
          .Start()) {
    using AccumulatorType = decltype(MakeAccumulator<Accumulator>{}(
        std::move(source_), std::forward<A>(a)...));
    auto acc = std::make_shared<AccumulatorType>(std::move(source_),
                                                 std::forward<A>(a)...);
    auto f = acc->Start();
    using future_t = decltype(f);
    // Extend lifetime of `acc` until the iteration completes.
    return f.then([acc](future_t g) { return g.get(); });
  }

 private:
  Source source_;
};

/**
 * Factory function to create a `SourceBuilder` for @p s
 */
template <typename Source>
auto make_source_builder(Source&& s) -> SourceBuilder<absl::decay_t<Source>> {
  return SourceBuilder<absl::decay_t<Source>>(std::forward<Source>(s));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_BUILDER_H
