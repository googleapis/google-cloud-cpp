// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PROXY_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PROXY_OPTIONS_H

#include "google/cloud/options.h"
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Set the proxy server address and port 
 *
 *
 * @ingroup storage-options
 */
struct ProxyServerAddressPortOption {
  using Type = std::pair<std::string, std::string>;
};

/**
 * Set the proxy server authenitication username and password
 *
 *
 * @ingroup storage-options
 */
struct ProxyServerCredentialsOption {
  using Type = std::pair<std::string, std::string>;
};


/**
 * A list of all the proxy options.
 */
using ProxyServerOptionList =
    OptionList<ProxyServerAddressPortOption, ProxyServerCredentialsOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PROXY_OPTIONS_H
