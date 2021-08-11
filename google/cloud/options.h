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

#include "google/cloud/internal/type_list.h"
#include "google/cloud/version.h"
#include "absl/types/any.h"
#include <set>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

class Options;
namespace internal {
Options MergeOptions(Options, Options);
void CheckExpectedOptionsImpl(std::set<std::type_index> const&, Options const&,
                              char const*);
}  // namespace internal

/**
 * A class that holds option structs indexed by their type.
 *
 * An "Option" is any struct that has a public `Type` member typedef. By
 * convention they are named like "FooOption". Each library (e.g., spanner,
 * storage) may define their own set of options. Additionally, there are common
 * options defined that many libraries may use. All these options may be set in
 * a single `Options` instance, and each library will look at the options that
 * it needs.
 *
 * Here's an overview of this class's interface, but see the method
 * documentation below for details.
 *
 * - `.set<T>(x)`    -- Sets the option `T` to value `x`
 * - `.has<T>()`     -- Returns true iff option `T` is set
 * - `.unset<T>()`   -- Removes the option `T`
 * - `.get<T>()`     -- Gets a const-ref to the value of option `T`
 * - `.lookup<T>(x)` -- Gets a non-const-ref to option `T`'s value, initializing
 *                      it to `x` if it was not set (`x` is optional).
 *
 * @par Example:
 *
 * @code
 * struct FooOption {
 *   using Type = int;
 * };
 * struct BarOption {
 *   using Type = std::set<std::string>;
 * };
 * ...
 * Options opts;
 *
 * assert(opts.get<FooOption>() == 0);
 * opts.set<FooOption>(42);
 * assert(opts.get<FooOption>() == 42);
 *
 * // Inserts two elements directly into the BarOption's std::set.
 * opts.lookup<BarOption>().insert("hello");
 * opts.lookup<BarOption>().insert("world");
 *
 * std::set<std::string> const& bar = opts.get<BarOption>();
 * assert(bar == std::set<std::string>{"hello", "world"});
 * @endcode
 */
class Options {
 private:
  template <typename T>
  using ValueTypeT = typename T::Type;

 public:
  /// Constructs an empty instance.
  Options() = default;

  Options(Options const&) = default;
  Options& operator=(Options const&) = default;
  Options(Options&&) = default;
  Options& operator=(Options&&) = default;

  /**
   * Sets option `T` to the value @p v and returns a reference to `*this`.
   *
   * @code
   * struct FooOption {
   *   using Type = int;
   * };
   * auto opts = Options{}.set<FooOption>(123);
   * @endcode
   *
   * @tparam T the option type
   * @param v the value to set the option T
   */
  template <typename T>
  Options& set(ValueTypeT<T> v) {
    m_[typeid(T)] = Data<T>{std::move(v)};
    return *this;
  }

  /**
   * Returns true IFF an option with type `T` exists.
   *
   * @tparam T the option type
   */
  template <typename T>
  bool has() const {
    return m_.find(typeid(T)) != m_.end();
  }

  /**
   * Erases the option specified by the type `T`.
   *
   * @tparam T the option type
   */
  template <typename T>
  void unset() {
    m_.erase(typeid(T));
  }

  /**
   * Returns a reference to the value for `T`, or a value-initialized default
   * if `T` was not set.
   *
   * This method will always return a reference to a valid value of the correct
   * type for option `T`, whether or not `T` has actually been set. Use
   * `has<T>()` to check whether or not the option has been set.
   *
   * @code
   * struct FooOption {
   *   using Type = std::set<std::string>;
   * };
   * Options opts;
   * std::set<std::string> const& x = opts.get<FooOption>();
   * assert(x.empty());
   * assert(!x.has<FooOption>());
   *
   * opts.set<FooOption>({"foo"});
   * assert(opts.get<FooOption>().size() == 1);
   * @endcode
   *
   * @tparam T the option type
   */
  template <typename T>
  ValueTypeT<T> const& get() const {
    static auto const* const kDefaultValue = new ValueTypeT<T>{};
    auto const it = m_.find(typeid(T));
    if (it != m_.end()) return absl::any_cast<Data<T>>(&it->second)->value;
    return *kDefaultValue;
  }

  /**
   * Returns a reference to the value for option `T`, setting the value to @p
   * init_value if necessary.
   *
   * @code
   * struct BigOption {
   *   using Type = std::set<std::string>;
   * };
   * Options opts;
   * std::set<std::string>& x = opts.lookup<BigOption>();
   * assert(x.empty());
   *
   * x.insert("foo");
   * opts.lookup<BigOption>().insert("bar");
   * assert(x.size() == 2);
   * @endcode
   *
   * @tparam T the option type
   * @param init_value the initial value to use if `T` is not set (optional)
   */
  template <typename T>
  ValueTypeT<T>& lookup(ValueTypeT<T> init_value = {}) {
    auto const p = m_.emplace(typeid(T), Data<T>{std::move(init_value)});
    return absl::any_cast<Data<T>>(&p.first->second)->value;
  }

 private:
  friend Options internal::MergeOptions(Options, Options);
  friend void internal::CheckExpectedOptionsImpl(
      std::set<std::type_index> const&, Options const&, char const*);

  // The data holder for all the option values.
  template <typename T>
  struct Data {
    ValueTypeT<T> value;
  };

  // The `absl::any` objects all hold a `Data<T>`
  std::unordered_map<std::type_index, absl::any> m_;
};

/**
 * A template to hold a list of "option" types.
 *
 * This can be a useful way to create meaningful lists of options. For example,
 * there could be a list containing all the gRPC options. Or a list of all
 * ProductX options. This gives us a way to link to lists of options with
 * doxygen, and to do some checking about what options a function may expect.
 */
template <typename... T>
using OptionList = internal::TypeList<T...>;

namespace internal {

// Wraps `T` in a `OptionList`, unless it was already one.
template <typename T>
struct WrapTypeList {
  using Type = OptionList<T>;
};
template <typename... T>
struct WrapTypeList<OptionList<T...>> {
  using Type = OptionList<T...>;  // Note: Doesn't work w/ nested OptionLists.
};
template <typename T>
using WrapTypeListT = typename WrapTypeList<T>::Type;

template <typename... T>
void CheckExpectedOptionsImpl(OptionList<T...> const&, Options const& opts,
                              char const* caller) {
  CheckExpectedOptionsImpl({typeid(T)...}, opts, caller);
}

/**
 * Checks that `Options` only contains the given expected options or a subset
 * of them.
 *
 * Logs all unexpected options. Note that logging is not always shown
 * on the console. Set the environment variable
 * `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` to enable logging.
 *
 * Options may be specified directly or as a collection in an `OptionList`.
 * For example,
 *
 * @code
 * struct FooOption { int value; };
 * struct BarOption { int value; };
 * using MyOptions = OptionList<FooOption, BarOption>;
 *
 * struct BazOption { int value; };
 *
 * // All valid ways to call this with varying expectations.
 * CheckExpectedOptions<FooOption>(opts, "test caller");
 * CheckExpectedOptions<FooOption, BarOption>(opts, "test caller");
 * CheckExpectedOptions<MyOptions>(opts, "test caller");
 * CheckExpectedOptions<BazOption, MyOptions>(opts, "test caller");
 * @endcode
 *
 * @param opts the `Options` to check.
 * @param caller some string indicating the callee function; logged IFF there's
 *        an unexpected option
 */
template <typename... T>
void CheckExpectedOptions(Options const& opts, char const* caller) {
  using ExpectedTypes = TypeListCatT<WrapTypeListT<T>...>;
  CheckExpectedOptionsImpl(ExpectedTypes{}, opts, caller);
}

/**
 * Moves the options from @p b into @p a and returns the result, unless the
 * option already exists in @p a.
 */
Options MergeOptions(Options a, Options b);

}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPTIONS_H
