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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_RESPONSE_READER_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_RESPONSE_READER_H_

#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/sync_stream.h>

namespace bigtable {
namespace testing {
/**
 * Refactor code common to several mock objects.
 *
 * Mocking a grpc::ClientReaderInterface<> was getting tedious.
 *
 * @tparam Response the response type.
 */
template <typename Response, typename Request>
class MockResponseReader : public grpc::ClientReaderInterface<Response> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1_T(Read, bool(Response*));
};

}  // namespace testing
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_RESPONSE_READER_H_
