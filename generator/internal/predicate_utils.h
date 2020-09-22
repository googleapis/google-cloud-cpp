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
template <typename T>
class GenericAll {
 public:
  template <typename... Predicates>
  explicit GenericAll(Predicates&&... p)
      : predicates_({std::forward<Predicates>(p)...}) {}

  bool operator()(T const& m) const {
    for (auto const& p : predicates_) {
      if (!p(m)) return false;
    }
    return true;
  }

 private:
  std::vector<std::function<bool(T const& m)>> predicates_;
};

using All = GenericAll<google::protobuf::MethodDescriptor>;

/**
 * Returns true if any predicate returns true.
 * @tparam T
 */
template <typename T>
class GenericAny {
 public:
  template <typename... Predicates>
  explicit GenericAny(Predicates&&... p)
      : predicates_({std::forward<Predicates>(p)...}) {}

  bool operator()(T const& m) const {
    for (auto const& p : predicates_) {
      if (p(m)) return true;
    }
    return false;
  }

 private:
  std::vector<std::function<bool(T const& m)>> predicates_;
};

using Any = GenericAny<google::protobuf::MethodDescriptor>;

/**
 * Returns true if both predicates return true.
 */
template <typename T>
class GenericAnd {
 public:
  GenericAnd(std::function<bool(T const& m)> lhs,
             std::function<bool(T const& m)> rhs)
      : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
  bool operator()(T const& m) const { return lhs_(m) && rhs_(m); }

 private:
  std::function<bool(T const& m)> lhs_;
  std::function<bool(T const& m)> rhs_;
};

using And = GenericAnd<google::protobuf::MethodDescriptor>;

/**
 * Returns true if either predicate returns true.
 */
template <typename T>
class GenericOr {
 public:
  GenericOr(std::function<bool(T const& m)> lhs,
            std::function<bool(T const& m)> rhs)
      : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
  bool operator()(T const& m) const { return lhs_(m) || rhs_(m); }

 private:
  std::function<bool(T const& m)> lhs_;
  std::function<bool(T const& m)> rhs_;
};

using Or = GenericOr<google::protobuf::MethodDescriptor>;

/**
 * Predicate negation operation.
 */
template <typename T>
class GenericNot {
 public:
  explicit GenericNot(std::function<bool(T const& m)> lhs)
      : lhs_(std::move(lhs)) {}
  bool operator()(T const& m) const { return !lhs_(m); }

 private:
  std::function<bool(T const& m)> lhs_;
};

using Not = GenericNot<google::protobuf::MethodDescriptor>;

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
