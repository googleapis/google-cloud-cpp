// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <grpcpp/client_context.h>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * Verify that the metadata in the context is appropriate for a gRPC method.
 *
 * `ClientContext` should instruct gRPC to set a `x-goog-request-params` HTTP
 * header with a value dictated by a `google.api.http` option in the gRPC
 * service specification. This function checks if the header is set and whether
 * it has a valid value.
 *
 * @param context the context to validate
 * @param method a gRPC method which which this context will be passed to
 * @param api_client_header expected value for the x-goog-api-client metadata
 *     header.
 * @param resource_prefix_header if specified, this is the expected value for
 *     the google-cloud-resource-prefix metadata header.
 *
 * @warning the `context` will be destroyed and shouldn't be used after passing
 *     it to this function.
 *
 * @return an OK status if the `context` is properly set up
 */
Status IsContextMDValid(
    grpc::ClientContext& context, std::string const& method,
    std::string const& api_client_header,
    absl::optional<std::string> const& resource_name = {},
    absl::optional<std::string> const& resource_prefix_header = {});

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H
