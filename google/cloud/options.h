// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPTIONS_H

#include "google/cloud/version.h"
#include "absl/types/any.h"
#include "absl/types/optional.h"
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

/**
 * A class that holds option structs indexed by their type.
 *
 * An "Option" can be any unique struct, but by convention these structs tend
 * to have a single data member named "value" and a name like "FooOption".
 * Each library (e.g., spanner, storage) may define their own set of options.
 * Additionally, various common classes may define options. All these options
 * may be set in a single `Options` instance, and each library will look at the
 * options that it needs.
 *
 * @par Example:
 *
 * @code
 * // Given
 * struct EndpointOption {
 *   std::string value;
 * };
 * struct FooOption {
 *   int value;
 * };
 * struct BarOption {
 *   double value;
 * };
 * ...
 * auto opts = Options{}
 *                 .set<EndpointOption>("blah.googleapis.com")
 *                 .set<FooOption>(42);
 * absl::optional<FooOption> foo = opts.get<FooOption>();
 * assert(foo.has_value());
 * assert(foo->value == 42);
 *
 * BarOption bar = opts.get_or<BarOption>(3.14);
 * assert(bar.value == 3.14);
 * @endcode
 */
class Options {
 public:
  /// Constructs an empty instance.
  Options() = default;

  Options(Options const&) = default;
  Options& operator=(Options const&) = default;
  Options(Options&&) = default;
  Options& operator=(Options&&) = default;

  /**
   * Returns true iff no options are currently set.
   */
  bool empty() const { return m_.empty(); }

  /**
   * Sets the specified option and returns a reference to `*this`.
   *
   * The optional arguments to `set(...)` will be used to construct the `T`
   * option.
   */
  template <typename T, typename... U>
  Options& set(U&&... u) {
    m_[typeid(T)] = T{std::forward<U>(u)...};
    return *this;
  }

  /**
   * Erases the option specified by the type `T`.
   */
  template <typename T>
  void unset() {
    m_.erase(typeid(T));
  }

  /**
   * Gets the option specified by its type `T`, if it was set.
   */
  template <typename T>
  absl::optional<T> get() const {
    auto it = m_.find(typeid(T));
    if (it == m_.end()) return absl::nullopt;
    return absl::any_cast<T>(it->second);
  }

  /**
   * Gets the option of type `T` if set, else a newly constructed default `T`.
   *
   * If the specified option `T` is not set, a new `T` will be constructed with
   * the optional arguments @p u.
   */
  template <typename T, typename... U>
  T get_or(U&&... u) const {
    auto v = get<T>();
    if (v) return *v;
    return T{std::forward<U>(u)...};
  }

  /**
   * Gets a pointer to the option specified by `T`, else nullptr.
   */
  template <typename T>
  T* get_pointer() {
    auto it = m_.find(typeid(T));
    if (it == m_.end()) return nullptr;
    return absl::any_cast<T>(&it->second);
  }

 private:
  std::unordered_map<std::type_index, absl::any> m_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPTIONS_H
