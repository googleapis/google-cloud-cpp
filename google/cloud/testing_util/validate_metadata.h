// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H

#include "google/cloud/rpc_metadata.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/protobuf/message.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

/**
 * Set server metadata on a `ClientContext`.
 *
 * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
 *   cannot reuse @p context for other RPCs or other calls to this function.
 */
void SetServerMetadata(grpc::ClientContext& context,
                       RpcMetadata const& server_metadata);

/**
 * Keep the test required to test metadata contents in a grpc::Context object.
 *
 * Our libraries need to set a number of metadata attributes in the
 * `grpc::ClientContext` objects used to make RPCs.  Naturally, we want to write
 * tests for the functions that set this metadata, but `grpc::ClientContext`
 * does not have any APIs to examine the metadata previously set.
 *
 * However, we can make a call to a *local* gRPC server, and then examine the
 * metadata on this server. Unfortunately, initializing these servers can be
 * slow, particularly on Windows.  It is worthwhile to cache the state and the
 * server in a fixture, so the tests run faster.
 *
 * If the code that implements this class is unfamiliar, the gRPC C++
 * Asynchronous-API tutorial is a good resource:
 * https://grpc.io/docs/languages/cpp/async/
 */
class ValidateMetadataFixture {
 public:
  ValidateMetadataFixture();
  ~ValidateMetadataFixture();

  /**
   * GetMetadata from `ClientContext`.
   *
   * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
   *   cannot reuse @p context for other RPCs or other calls to this function.
   */
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& client_context);

  /**
   * Get the `authority` field from `ClientContext`.
   *
   * With older versions of gRPC this returns `absl::nullopt`, the caller may
   * want to skip the rest of the test.
   *
   * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
   *   cannot reuse @p context for other RPCs or other calls to this function.
   */
  absl::optional<std::string> GetAuthority(grpc::ClientContext& client_context);

  /**
   * Set server metadata on a `ClientContext`.
   *
   * @note A `grpc::ClientContext` can be used in only one gRPC. The caller
   *   cannot reuse @p context for other RPCs or other calls to this function.
   */
  void SetServerMetadata(grpc::ClientContext& client_context,
                         RpcMetadata const& server_metadata = {});

  /**
   * Verify that the metadata in the context is appropriate for a gRPC method.
   *
   * `ClientContext` should instruct gRPC to set a `x-goog-request-params` HTTP
   * header with a value determined by the `google.api.routing` or
   * `google.api.http` option in the gRPC service specification. This function
   * checks if the header is set and whether it has a valid value.
   *
   * @note A `grpc::ClientContext` can be used in only one RPC. The caller
   *   cannot reuse @p context for other RPCs or other calls to this function.
   *
   * @param context the context to validate
   * @param method a gRPC method which which this context will be passed to
   * @param api_client_header expected value for the x-goog-api-client metadata
   *     header.
   * @param resource_prefix_header if specified, this is the expected value for
   *     the google-cloud-resource-prefix metadata header.
   */
  void IsContextMDValid(
      grpc::ClientContext& context, std::string const& method_name,
      google::protobuf::Message const& request,
      std::string const& api_client_header,
      absl::optional<std::string> const& resource_name = {},
      absl::optional<std::string> const& resource_prefix_header = {});

 private:
  /**
   * Performs a fake call, leaving the client's metadata accessible from the
   * server context, and vice versa.
   */
  void ExchangeMetadata(grpc::ClientContext& client_context,
                        grpc::GenericServerContext& server_context);

  grpc::CompletionQueue cli_cq_;
  grpc::AsyncGenericService generic_service_;
  std::unique_ptr<grpc::ServerCompletionQueue> srv_cq_;
  std::unique_ptr<grpc::Server> server_;
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_VALIDATE_METADATA_H
