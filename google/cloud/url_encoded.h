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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_URL_ENCODED_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_URL_ENCODED_H

#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Encodes the given string and returns a new string such that all input
// characters that are not in [a-zA-z-._~] are converted to their "URL escaped"
// version "%NN" where NN is the ASCII hex value.
std::string UrlEncode(std::string const& input);

// Decodes the given string, interpreting any "%NN" sequence as an "URL escaped"
// character and substitutes the ASCII character corresponding to hex value NN.
std::string UrlDecode(std::string const& input);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_URL_ENCODED_H
