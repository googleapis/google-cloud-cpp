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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include <iostream>

int main() {
  // Adding openssl to the global namespace when the user does not explicitly
  // asks for it is too much namespace pollution. The application may not want
  // that many dependencies. Also, on Windows that may drag really unwanted
  // dependencies.
#ifdef OPENSSL_VERSION_NUMBER
#error "OPENSSL should not be included by storage public headers"
#endif  // OPENSSL_VERSION_NUMBER

  // Adding libcurl to the global namespace when the user does not explicitly
  // asks for it is too much namespace pollution. The application may not want
  // that many dependencies. Also, on Windows that may drag really unwanted
  // dependencies.
#ifdef LIBCURL_VERSION
#error "LIBCURL should not be included by storage public headers"
#endif  // OPENSSL_VERSION_NUMBER

  // When we include the storage headers we do not want to leave the include
  // guards for nlohmann::json defined, that stops our users from including
  // similar headers.
#ifdef NLOHMANN_JSON_FWD_HPP
#error "NLOHMANN_JSON_FWD_HPP should not be left defined."
#endif  // NLOHMANN_JSON_FWD_HPP
#ifdef NLOHMANN_JSON_HPP
#error "NLOHMANN_JSON_HPP should not be left defined."
#endif  // NLOHMANN_JSON_HPP

  std::cout << "PASSED: this is a compile-time test\n";
  return 0;
}
