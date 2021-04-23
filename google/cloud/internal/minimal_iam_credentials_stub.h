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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MINIMAL_IAM_CREDENTIALS_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MINIMAL_IAM_CREDENTIALS_STUB_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <google/iam/credentials/v1/iamcredentials.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * A wrapper for the IAM Credentials Stub.
 *
 * We cannot use the micro-generated classes because:
 *
 * * We need to support asynchronous operations, and the micro-generator does
 *   not yet generate async functions.
 * * We do not want a retry loop, any (transient) failures should be retried by
 *   the caller.
 * * Furthermore, using the micro-generated classes would introduce a cycle:
 *   - the `google-cloud-cpp::grpc_utils` library would depend on the micro
 *     generated library
 *   - the micro-generated libraries always depend on
 *     `google-cloud-cpp::grpc_utils`.
 */
class MinimalIamCredentialsStub {
 public:
  virtual ~MinimalIamCredentialsStub() = default;

  virtual future<
      StatusOr<::google::iam::credentials::v1::GenerateAccessTokenResponse>>
  AsyncGenerateAccessToken(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      ::google::iam::credentials::v1::GenerateAccessTokenRequest const&
          request) = 0;
};

/// Mostly used for unit testing, adds the metadata and logging decorators.
std::shared_ptr<MinimalIamCredentialsStub> DecorateMinimalIamCredentialsStub(
    std::shared_ptr<MinimalIamCredentialsStub> impl, Options const& options);

/**
 * Creates an instance of MinimalIamCredentialStub.
 *
 * Creates a functional stub, including all the decorators.
 */
std::shared_ptr<MinimalIamCredentialsStub> MakeMinimalIamCredentialsStub(
    std::shared_ptr<GrpcAuthenticationStrategy> auth_strategy,
    Options const& options);

Options MakeMinimalIamCredentialsOptions(Options options);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_MINIMAL_IAM_CREDENTIALS_STUB_H
