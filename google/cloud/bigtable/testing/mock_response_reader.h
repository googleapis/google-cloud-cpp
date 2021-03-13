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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_RESPONSE_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_RESPONSE_READER_H

#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/sync_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
/**
 * Refactor code common to several mock objects.
 *
 * Mocking a grpc::ClientReaderInterface<> was getting tedious. This refactors
 * most (but unfortunately cannot refactor all) the code for such objects.
 *
 * @tparam Response the response type.
 */
template <typename Response, typename Request>
class MockResponseReader : public grpc::ClientReaderInterface<Response> {
 public:
  explicit MockResponseReader(std::string method)
      : method_(std::move(method)) {}
  MOCK_METHOD(void, WaitForInitialMetadata, (), (override));
  MOCK_METHOD(grpc::Status, Finish, (), (override));
  MOCK_METHOD(bool, NextMessageSize, (std::uint32_t*), (override));
  MOCK_METHOD(bool, Read, (Response*), (override));

  using UniquePtr = std::unique_ptr<grpc::ClientReaderInterface<Response>>;

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
    return [this](grpc::ClientContext* context, Request const&) {
      EXPECT_STATUS_OK(google::cloud::testing_util::IsContextMDValid(
          *context, method_, google::cloud::internal::ApiClientHeader()));
      return UniquePtr(this);
    };
  }

 private:
  std::string method_;
};

/**
 * Define the interface to mock the result of starting a unary async RPC.
 *
 * Note that using this mock often requires special memory management. The
 * google mock library requires all mocks to be destroyed. In contrast, grpc
 * specializes `std::unique_ptr<>` to *not* delete objects of type
 * `grpc::ClientAsyncResponseReaderInterface<T>`:
 *
 *
 *     https://github.com/grpc/grpc/blob/608188c680961b8506847c135b5170b41a9081e8/include/grpcpp/impl/codegen/async_unary_call.h#L305
 *
 * No delete, no destructor, nothing. The gRPC library expects all
 * `grpc::ClientAsyncResponseReader<R>` objects to be allocated from a
 * per-call arena, and deleted in bulk with other objects when the call
 * completes and the full arena is released. Unfortunately, our mocks are
 * allocated from the global heap, as they do not have an associated call or
 * arena. The override in the gRPC library results in a leak, unless we manage
 * the memory explicitly.
 *
 * As a result, the unit tests need to manually delete the objects. The idiom we
 * use is a terrible, horrible, no good, very bad hack:
 *
 * We create a unique pointer to `MockAsyncResponseReader<T>`, then we pass that
 * pointer to gRPC using `reader.get()`, and gRPC promptly puts it into a
 * `std::unique_ptr<ClientAsyncResponseReaderInterface<T>>`. That looks like a
 * double delete waiting to happen, but it is not, because gRPC has done the
 * weird specialization of `std::unique_ptr`.
 *
 * @tparam Response the type of the RPC response
 */
template <typename Response>
class MockAsyncResponseReader
    : public grpc::ClientAsyncResponseReaderInterface<Response> {
 public:
  MOCK_METHOD(void, StartCall, (), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
  MOCK_METHOD(void, Finish, (Response*, grpc::Status*, void*), (override));
};

template <typename Response>
class MockClientAsyncReaderInterface
    : public grpc::ClientAsyncReaderInterface<Response> {
 public:
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, Read, (Response*, void*), (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_RESPONSE_READER_H
