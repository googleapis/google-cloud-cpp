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
template <typename Derived, typename Option, typename... Options>
class GenericRequestBase;

/**
 * Refactor common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `RequestParameters`, it is the
 * base class for a hierarchy of `GenericRequestBase<Parameters...>` when the
 * list has a single parameter.
 *
 * @tparam Option the type of the option contained in this object.
 */
template <typename Derived, typename Option>
class GenericRequestBase<Derived, Option> {
 public:
  Derived& set_option(Option&& p) {
    option_ = std::move(p);
    return *static_cast<Derived*>(this);
  }

  template <typename HttpRequest>
  void AddOptionsToHttpRequest(HttpRequest& request) const {
    request.AddOption(option_);
  }

  void DumpOptions(std::ostream& os, char const* sep) const {
    if (option_.has_value()) {
      os << sep << option_;
    }
  }

 private:
  Option option_;
};

/**
 * Refactor common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `RequestParameters` see below for
 * more details.
 */
template <typename Derived, typename Option, typename... Options>
class GenericRequestBase : public GenericRequestBase<Derived, Options...> {
 public:
  using GenericRequestBase<Derived, Options...>::set_option;

  Derived& set_option(Option&& p) {
    option_ = std::move(p);
    return *static_cast<Derived*>(this);
  }

  template <typename HttpRequest>
  void AddOptionsToHttpRequest(HttpRequest& request) const {
    request.AddOption(option_);
    GenericRequestBase<Derived, Options...>::AddOptionsToHttpRequest(request);
  }

  void DumpOptions(std::ostream& os, char const* sep) const {
    if (option_.has_value()) {
      os << sep << option_;
      GenericRequestBase<Derived, Options...>::DumpOptions(os, ", ");
    } else {
      GenericRequestBase<Derived, Options...>::DumpOptions(os, sep);
    }
  }

 private:
  Option option_;
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
 * class FooRequest : private internal::RequestOptions<UserProject, P1, P2>
 * @endcode
 *
 * 2) Define a generic function to set a parameter:
 * @code
 * class FooRequest // some things ommitted
 * {
 *   template <typename Parameter>
 *   FooRequest& set_parameter(Parameter&& p) {
 *     RequestOptions::set_parameter(p);
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
 *   template <typename... Options>
 *   FooRequest& set_multiple_options(Options&&... p) {
 *     RequestOptions::set_multiple_options(std::forward<Parameter>(p)...);
 *     return *this;
 *   }
 * @endcode
 *
 * @tparam Options the list of options that the Request class will support.
 */
template <typename Derived, typename... Options>
class GenericRequest : public GenericRequestBase<Derived, Options...> {
 public:
  template <typename H, typename... T>
  Derived& set_multiple_options(H&& h, T&&... tail) {
    GenericRequestBase<Derived, Options...>::set_option(std::forward<H>(h));
    return set_multiple_options(std::forward<T>(tail)...);
  }

  Derived& set_multiple_options() { return *static_cast<Derived*>(this); }
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GENERIC_REQUEST_H_
