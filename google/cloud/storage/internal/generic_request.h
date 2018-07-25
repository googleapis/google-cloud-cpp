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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_REQUEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_REQUEST_H_

#include "google/cloud/storage/version.h"
#include <iostream>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
// Forward declare the template so we can specialize it first. Defining the
// specialization first, which is the base class, should be more readable.
template <typename Derived, typename Modifier, typename... Modifiers>
class GenericRequestBase;

/**
 * Refactor common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `RequestParameters`, it is the
 * base class for a hierarchy of `GenericRequestBase<Parameters...>` when the
 * list has a single parameter.
 *
 * @tparam Modifier the parameter contained in this object.
 */
template <typename Derived, typename Modifier>
class GenericRequestBase<Derived, Modifier> {
 public:
  Derived& set_modifier(Modifier&& p) {
    modifier_ = std::move(p);
    return *static_cast<Derived*>(this);
  }

  template <typename HttpRequest>
  void AddModifiersToHttpRequest(HttpRequest& request) const {
    request.AddModifier(modifier_);
  }

  void DumpModifiers(std::ostream& os, char const* sep) const {
    if (modifier_.has_value()) {
      os << sep << modifier_;
    }
  }

 private:
  Modifier modifier_;
};

/**
 * Refactor common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `RequestParameters` see below for
 * more details.
 */
template <typename Derived, typename Modifier, typename... Modifiers>
class GenericRequestBase : public GenericRequestBase<Derived, Modifiers...> {
 public:
  using GenericRequestBase<Derived, Modifiers...>::set_modifier;

  Derived& set_modifier(Modifier&& p) {
    modifier_ = std::move(p);
    return *static_cast<Derived*>(this);
  }

  template <typename HttpRequest>
  void AddModifiersToHttpRequest(HttpRequest& request) const {
    request.AddModifier(modifier_);
    GenericRequestBase<Derived, Modifiers...>::AddModifiersToHttpRequest(
        request);
  }

  void DumpModifiers(std::ostream& os, char const* sep) const {
    if (modifier_.has_value()) {
      os << sep << modifier_;
      GenericRequestBase<Derived, Modifiers...>::DumpModifiers(os, ", ");
    } else {
      GenericRequestBase<Derived, Modifiers...>::DumpModifiers(os, sep);
    }
  }

 private:
  Modifier modifier_;
};

/**
 * Refactor common functions to operate on optional request parameters.
 *
 * Each operation in the client library has its own `*Request` class, and each
 * of these classes needs to define functions to change the optional parameters
 * of the request. This class implements these functions in a single place,
 * saving us a lot typing.
 *
 * To implement `FooRequest` you do three things:
 *
 * 1) Make this class a (private) base class of `FooRequest`, with the list of
 *    optional parameters it will support:
 * @code
 * class FooRequest : private internal::RequestModifiers<UserProject, P1, P2>
 * @endcode
 *
 * 2) Define a generic function to set a parameter:
 * @code
 * class FooRequest // some things ommitted
 * {
 *   template <typename Parameter>
 *   FooRequest& set_parameter(Parameter&& p) {
 *     RequestModifiers::set_parameter(p);
 *     return *this;
 *   }
 * @endcode
 *
 * Note that this is a single function, but only the parameter types declared
 * in the base class are accepted.
 *
 * 3) Define a generic function to set multiple parameters:
 * @code
 * class FooRequest // some things ommitted
 * {
 *   template <typename... Modifiers>
 *   FooRequest& set_multiple_modifiers(Modifiers&&... p) {
 *     RequestModifiers::set_multiple_modifiers(std::forward<Parameter>(p)...);
 *     return *this;
 *   }
 * @endcode
 *
 * @tparam Modifiers the list of parameters that the Request class will
 *     support.
 */
template <typename Derived, typename... Modifiers>
class GenericRequest : public GenericRequestBase<Derived, Modifiers...> {
 public:
  template <typename H, typename... T>
  Derived& set_multiple_modifiers(H&& h, T&&... tail) {
    GenericRequestBase<Derived, Modifiers...>::set_modifier(std::forward<H>(h));
    return set_multiple_modifiers(std::forward<T>(tail)...);
  }

  Derived& set_multiple_modifiers() { return *static_cast<Derived*>(this); }
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_REQUEST_H_
