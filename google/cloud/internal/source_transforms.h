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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_TRANSFORMS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_TRANSFORMS_H

#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/source_ready_token.h"
#include "absl/meta/type_traits.h"
#include "absl/types/variant.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <typename Source, typename Transform>
class TransformedSource {
 public:
  TransformedSource(Source s, Transform t)
      : source_(std::move(s)), transform_(std::move(t)) {}

  //@{
  using value_type = invoke_result_t<Transform, typename Source::value_type>;
  using error_type = typename Source::error_type;

  auto ready() -> decltype(std::declval<Source>().ready()) {
    return source_.ready();
  }

  future<absl::variant<value_type, error_type>> next(ReadyToken t) {
    using source_value_type = typename Source::value_type;
    using source_event_type = absl::variant<source_value_type, error_type>;
    using event_type = absl::variant<value_type, error_type>;

    return source_.next(std::move(t)).then([this](future<source_event_type> f) {
      struct Visitor {
        TransformedSource* self;

        event_type operator()(source_value_type v) {
          return self->transform_(std::move(v));
        }
        event_type operator()(error_type e) { return e; }
      };
      return absl::visit(Visitor{this}, f.get());
    });
  }
  //@}

 private:
  Source source_;
  Transform transform_;
};

template <typename Source, typename Transform>
TransformedSource<absl::decay_t<Source>, absl::decay_t<Transform>>
MakeTransformedSource(Source&& s, Transform&& t) {
  return TransformedSource<absl::decay_t<Source>, absl::decay_t<Transform>>(
      std::forward<Source>(s), std::forward<Transform>(t));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_TRANSFORMS_H
