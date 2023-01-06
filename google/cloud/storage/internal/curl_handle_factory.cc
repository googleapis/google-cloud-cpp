// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/curl_handle_factory.h"
#include "google/cloud/storage/options.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::shared_ptr<rest_internal::CurlHandleFactory>
GetDefaultCurlHandleFactory() {
  static auto const* const kFactory =
      new auto(std::make_shared<rest_internal::DefaultCurlHandleFactory>());
  return *kFactory;
}

std::shared_ptr<rest_internal::CurlHandleFactory> GetDefaultCurlHandleFactory(
    Options const& options) {
  if (!options.get<CARootsFilePathOption>().empty()) {
    return std::make_shared<rest_internal::DefaultCurlHandleFactory>(options);
  }
  return GetDefaultCurlHandleFactory();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
