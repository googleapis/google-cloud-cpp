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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_MINIMAL_IAM_CREDENTIALS_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_MINIMAL_IAM_CREDENTIALS_STUB_H

#include "google/cloud/internal/minimal_iam_credentials_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

class MockMinimalIamCredentialsStub
    : public google::cloud::internal::MinimalIamCredentialsStub {
 public:
  MOCK_METHOD(
      future<
          StatusOr<google::iam::credentials::v1::GenerateAccessTokenResponse>>,
      AsyncGenerateAccessToken,
      (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
       google::iam::credentials::v1::GenerateAccessTokenRequest const&),
      (override));
  MOCK_METHOD(StatusOr<google::iam::credentials::v1::SignBlobResponse>,
              SignBlob,
              (grpc::ClientContext&,
               google::iam::credentials::v1::SignBlobRequest const&),
              (override));
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_MINIMAL_IAM_CREDENTIALS_STUB_H
