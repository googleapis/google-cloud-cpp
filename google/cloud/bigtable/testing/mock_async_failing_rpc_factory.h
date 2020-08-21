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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ASYNC_FAILING_RPC_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ASYNC_FAILING_RPC_FACTORY_H

#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/support/async_unary_call.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/**
 * Helper class to create the expectations for a failing async RPC call.
 *
 * Given the type of the request and responses, this struct provides a function
 * to create a mock implementation with the right signature and checks.
 *
 * @tparam RequestType the protobuf type for the request.
 * @tparam ResponseType the protobuf type for the response.
 */
template <typename RequestType, typename ResponseType>
struct MockAsyncFailingRpcFactory {
  using SignatureType =
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<ResponseType>>(
          grpc::ClientContext* context, RequestType const& request,
          grpc::CompletionQueue*);

  MockAsyncFailingRpcFactory()
      : reader(new google::cloud::bigtable::testing::MockAsyncResponseReader<
               ResponseType>) {}

  /// Refactor the boilerplate common to most tests.
  std::function<SignatureType> Create(std::string const& expected_request,
                                      std::string const& method) {
    return std::function<SignatureType>([expected_request, method, this](
                                            grpc::ClientContext* context,
                                            RequestType const& request,
                                            grpc::CompletionQueue*) {
      using ::testing::_;
      EXPECT_STATUS_OK(google::cloud::testing_util::IsContextMDValid(
          *context, method, google::cloud::internal::ApiClientHeader()));
      RequestType expected;
      // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
      EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
          expected_request, &expected));
      std::string delta;
      google::protobuf::util::MessageDifferencer differencer;
      differencer.ReportDifferencesToString(&delta);
      EXPECT_TRUE(differencer.Compare(expected, request)) << delta;

      EXPECT_CALL(*reader, Finish(_, _, _))
          .WillOnce([](ResponseType* response, grpc::Status* status, void*) {
            EXPECT_NE(nullptr, response);
            *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "nooo");
          });
      // This is safe, see comments in MockAsyncResponseReader.
      return std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<ResponseType>>(reader.get());
    });
  }

  std::unique_ptr<
      google::cloud::bigtable::testing::MockAsyncResponseReader<ResponseType>>
      reader;
};
}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ASYNC_FAILING_RPC_FACTORY_H
