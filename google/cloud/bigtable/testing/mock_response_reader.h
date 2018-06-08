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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_RESPONSE_READER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_RESPONSE_READER_H_

#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/sync_stream.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
/**
 * Refactor code common to several mock objects.
 *
 * Mocking a grpc::ClientReaderInterface<> was getting tedious. This refactors
 * most (but unfortunately cannnot refactor all) the code for such objects.
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

  using UniquePtr = std::unique_ptr<grpc::ClientReaderInterface<Response>>;

  /// Return a `std::unique_ptr< mocked-class >`
  UniquePtr AsUniqueMocked() { return UniquePtr(this); }

  /**
   * Create a lambda that returns a `std::unique_ptr< mocked-class >`.
   *
   * Often the test code has to create a lambda that returns one of these mocks
   * wrapped in the correct (the base class) `std::unique_ptr<>`.
   *
   * We cannot use just `::testing::Return()` because that binds to the static
   * type of the returned object, and we need to return a `std::unique_ptr<Foo>`
   * where we have a `MockFoo*`.  And we cannot create a `std::unique_ptr<>`
   * and pass it because `::testing::Return()` assumes copy constructions and
   * `std::unique_ptr<>` only supports move constructors.
   */
  std::function<UniquePtr(grpc::ClientContext*, Request const&)>
  MakeMockReturner() {
    return [this](grpc::ClientContext*, Request const&) {
      return UniquePtr(this);
    };
  };
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_RESPONSE_READER_H_
