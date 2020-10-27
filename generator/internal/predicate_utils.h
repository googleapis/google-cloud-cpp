// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PREDICATE_UTILS_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PREDICATE_UTILS_H

#include "google/cloud/optional.h"
#include <google/protobuf/descriptor.h>
#include <functional>
#include <vector>

namespace google {
namespace cloud {
namespace generator_internal {

/**
 * Determines if the given method meets the criteria for pagination.
 *
 * https://google.aip.dev/client-libraries/4233
 */
bool IsPaginated(google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the given method has neither client-side streaming, server-side
 * streaming, nor bidirectional streaming.
 */
bool IsNonStreaming(google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the given method is a long running operation.
 */
bool IsLongrunningOperation(google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the given method returns `google::protobug::Empty`.
 */
bool IsResponseTypeEmpty(google::protobuf::MethodDescriptor const& method);

/**
 * Determines if the given method's response is contained in the longrunning
 * metadata field.
 */
bool IsLongrunningMetadataTypeUsedAsResponse(
    google::protobuf::MethodDescriptor const& method);
/**
 * If method meets pagination criteria, provides paginated field type and field
 * name.
 *
 * https://google.aip.dev/client-libraries/4233
 */
google::cloud::optional<std::pair<std::string, std::string>>
DeterminePagination(google::protobuf::MethodDescriptor const& method);

/**
 * Returns true if all predicates return true.
 */
template <typename T, typename... Ps>
std::function<bool(T const&)> GenericAll(Ps&&... p) {
  using function = std::function<bool(T const&)>;
  std::vector<function> predicates({std::forward<Ps>(p)...});
  return [predicates](T const& m) {
    return std::all_of(predicates.begin(), predicates.end(),
                       [&m](function const& p) { return p(m); });
  };
}

template <typename... Ps>
std::function<bool(google::protobuf::MethodDescriptor const&)> All(Ps&&... p) {
  return GenericAll<google::protobuf::MethodDescriptor, Ps...>(
      std::forward<Ps>(p)...);
}

/**
 * Returns true if any predicate returns true.
 * @tparam T
 */
template <typename T, typename... Ps>
std::function<bool(T const&)> GenericAny(Ps&&... p) {
  using function = std::function<bool(T const&)>;
  std::vector<function> predicates({std::forward<Ps>(p)...});
  return [predicates](T const& m) {
    return std::any_of(predicates.begin(), predicates.end(),
                       [&m](function const& p) { return p(m); });
  };
}

template <typename... Ps>
std::function<bool(google::protobuf::MethodDescriptor const&)> Any(Ps&&... p) {
  return GenericAny<google::protobuf::MethodDescriptor, Ps...>(
      std::forward<Ps>(p)...);
}

/**
 * Returns true if both predicates return true.
 */
template <typename T>
std::function<bool(T const&)> GenericAnd(
    std::function<bool(T const&)> const& lhs,
    std::function<bool(T const&)> const& rhs) {
  return [lhs, rhs](T const& m) -> bool { return lhs(m) && rhs(m); };
}

inline std::function<bool(google::protobuf::MethodDescriptor const&)> And(
    std::function<bool(google::protobuf::MethodDescriptor const&)> const& lhs,
    std::function<bool(google::protobuf::MethodDescriptor const&)> const& rhs) {
  return GenericAnd(lhs, rhs);
}

/**
 * Returns true if either predicate returns true.
 */
template <typename T>
std::function<bool(T const&)> GenericOr(
    std::function<bool(T const&)> const& lhs,
    std::function<bool(T const&)> const& rhs) {
  return [lhs, rhs](T const& m) -> bool { return lhs(m) || rhs(m); };
}

inline std::function<bool(google::protobuf::MethodDescriptor const&)> Or(
    std::function<bool(google::protobuf::MethodDescriptor const&)> const& lhs,
    std::function<bool(google::protobuf::MethodDescriptor const&)> const& rhs) {
  return GenericOr(lhs, rhs);
}

/**
 * Predicate negation operation.
 */
template <typename T>
std::function<bool(T const&)> GenericNot(
    std::function<bool(T const&)> const& p) {
  return [p](T const& m) -> bool { return !p(m); };
}

inline std::function<bool(google::protobuf::MethodDescriptor const&)> Not(
    std::function<bool(google::protobuf::MethodDescriptor const&)> const& p) {
  return GenericNot(p);
}

/**
 * When provided with two strings and a predicate, returns one of the strings
 * based on evaluation of predicate.
 *
 * When provided with one string, always returns the string.
 */
template <typename T>
class PredicatedFragment {
 public:
  using PredicateFn = std::function<bool(T const&)>;

  PredicatedFragment(PredicateFn predicate, std::string fragment_if_true,
                     std::string fragment_if_false)
      : predicate_(std::move(predicate)),
        fragment_if_true_(std::move(fragment_if_true)),
        fragment_if_false_(std::move(fragment_if_false)) {}

  PredicatedFragment(std::string fragment_always_true)  // NOLINT
      : predicate_([](T const&) { return true; }),
        fragment_if_true_(std::move(fragment_always_true)),
        fragment_if_false_({}) {}

  std::string operator()(T const& descriptor) const {
    if (predicate_(descriptor)) {
      return fragment_if_true_;
    }
    return fragment_if_false_;
  }

 private:
  PredicateFn predicate_;
  std::string fragment_if_true_;
  std::string fragment_if_false_;
};

template <typename T>
class Pattern {
 public:
  Pattern(std::vector<PredicatedFragment<T>> f, std::function<bool(T const&)> p)
      : fragments_(std::move(f)), predicate_(std::move(p)) {}

  bool operator()(T const& p) const { return predicate_(p); }
  std::vector<PredicatedFragment<T>> const& fragments() const {
    return fragments_;
  }

 private:
  std::vector<PredicatedFragment<T>> fragments_;
  std::function<bool(T const&)> predicate_;
};

using MethodPattern = Pattern<google::protobuf::MethodDescriptor>;

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTERNAL_PREDICATE_UTILS_H
