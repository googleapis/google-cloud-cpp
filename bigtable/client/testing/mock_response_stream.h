// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_RESPONSE_STREAM_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_RESPONSE_STREAM_H_

#include <google/bigtable/v2/bigtable.pb.h>
#include <gmock/gmock.h>

namespace bigtable {
namespace testing {
class MockResponseStream : public grpc::ClientReaderInterface<
                               ::google::bigtable::v2::ReadRowsResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1(Read, bool(::google::bigtable::v2::ReadRowsResponse*));
};

}  // namespace testing
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_MOCK_RESPONSE_STREAM_H_
