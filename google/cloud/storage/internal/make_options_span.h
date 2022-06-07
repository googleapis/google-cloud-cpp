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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_MAKE_OPTIONS_SPAN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_MAKE_OPTIONS_SPAN_H

#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

///@{
/**
 * The `GroupOptions()` overload set groups all the `google::cloud::Options`
 * present in a parameter pack into a single `google::cloud::Options`.
 *
 * @note This does not support `volative`-qualified references.
 */
inline google::cloud::Options GroupOptions() {
  return google::cloud::Options{};
}

template <typename... Tail>
static Options GroupOptions(Options& bundle, Tail&&... t) {
  return google::cloud::internal::MergeOptions(
      GroupOptions(std::forward<Tail>(t)...), std::move(bundle));
}

template <typename... Tail>
static Options GroupOptions(Options&& bundle, Tail&&... t) {
  return google::cloud::internal::MergeOptions(
      GroupOptions(std::forward<Tail>(t)...), std::move(bundle));
}

template <typename... Tail>
static Options GroupOptions(Options const& bundle, Tail&&... t) {
  return google::cloud::internal::MergeOptions(
      GroupOptions(std::forward<Tail>(t)...), std::move(bundle));
}

template <typename... Tail>
static Options GroupOptions(Options const&& bundle, Tail&&... t) {
  return google::cloud::internal::MergeOptions(
      GroupOptions(std::forward<Tail>(t)...), std::move(bundle));
}

template <typename Head, typename... Tail>
static Options GroupOptions(Head&&, Tail&&... t) {
  return GroupOptions(std::forward<Tail>(t)...);
}
///@}

/**
 * Create an option span for a `google::cloud::storage::Client` operation.
 *
 * All operations in `google::cloud::storage::Client` should create an option
 * span combining the options in the `RawClient` with any
 * `google::cloud::Options` in the parameter pack.
 */
template <typename... RequestOptions>
google::cloud::internal::OptionsSpan MakeOptionsSpan(
    Options defaults,  // NOLINT(performance-unnecessary-value-param)
    RequestOptions&&... o) {
  return google::cloud::internal::OptionsSpan(
      google::cloud::internal::MergeOptions(
          GroupOptions(std::forward<RequestOptions>(o)...),
          std::move(defaults)));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_MAKE_OPTIONS_SPAN_H
