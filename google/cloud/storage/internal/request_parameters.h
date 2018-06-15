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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REQUEST_PARAMETERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REQUEST_PARAMETERS_H_

#include "google/cloud/storage/version.h"
#include <utility>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
// Forward declare the template so we can specialize it first. Defining the
// specialization first, which is the base class, should be more readable.
template <typename Parameter, typename... Parameters>
class RequestParameterList;

/**
 * Refactor common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `RequestParameters`, it is the
 * base class for a hierarchy of `RequestParameterList<Parameters...>` when the
 * list has a single parameter.
 *
 * @tparam Parameter the parameter contained in this object.
 */
template <typename Parameter>
class RequestParameterList<Parameter> {
 public:
  void set_parameter(Parameter&& p) { parameter_ = std::move(p); }

  template <typename HttpRequest>
  void AddParametersToHttpRequest(HttpRequest& request) const {
    request.AddWellKnownParameter(parameter_);
  }

 private:
  Parameter parameter_;
};

/**
 * Refactor common functions that manipulate list of parameters in a request.
 *
 * This class is used in the implementation of `RequestParameters` see below for
 * more details.
 */
template <typename Parameter, typename... Parameters>
class RequestParameterList : public RequestParameterList<Parameters...> {
 public:
  using RequestParameterList<Parameters...>::set_parameter;

  void set_parameter(Parameter&& p) { parameter_ = std::move(p); }

  template <typename HttpRequest>
  void AddParametersToHttpRequest(HttpRequest& request) const {
    request.AddWellKnownParameter(parameter_);
    RequestParameterList<Parameters...>::AddParametersToHttpRequest(request);
  }

 private:
  Parameter parameter_;
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
 * class FooRequest : private internal::RequestParameters<UserProject, P1, P2>
 * @endcode
 *
 * 2) Define a generic function to set a parameter:
 * @code
 * class FooRequest // some things ommitted
 * {
 *   template <typename Parameter>
 *   FooRequest& set_parameter(Parameter&& p) {
 *     RequestParameters::set_parameter(p);
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
 *   template <typename... Parameters>
 *   FooRequest& set_multiple_parameters(Parameters&&... p) {
 *     RequestParameters::set_multiple_parameters(std::forward<Parameter>(p)...);
 *     return *this;
 *   }
 * @endcode
 *
 * @tparam Parameters the list of parameters that the Request class will
 *     support.
 */
template <typename... Parameters>
class RequestParameters : public RequestParameterList<Parameters...> {
 public:
  template <typename H, typename... T>
  void set_multiple_parameters(H&& h, T&&... tail) {
    RequestParameterList<Parameters...>::set_parameter(std::forward<H>(h));
    set_multiple_parameters(std::forward<T>(tail)...);
  }

  void set_multiple_parameters() {}
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_REQUEST_PARAMETERS_H_
