// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_ASYNC_RESPONSE_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_ASYNC_RESPONSE_READER_H

#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/async_unary_call.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * Define the interface to mock the result of starting a unary async RPC.
 *
 * Note that using this mock often requires special memory management. The
 * google mock library requires all mocks to be destroyed. In contrast, grpc
 * previously specialized `std::unique_ptr<>` to *not* delete objects of type
 * `grpc::ClientAsyncResponseReaderInterface<T>` (and our override of the
 * Destroy method below preserves that behavior even after
 * https://github.com/grpc/proposal/pull/238):
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

  // Preserve the behavior from before https://github.com/grpc/proposal/pull/238
  // of not destroying the object when
  // std::unique_ptr<grpc::ClientAsyncResponseReaderInterface> goes out of
  // scope.
  //
  // TODO(#6566): mark this with the override keyword once the method exists in
  // the parent class.
  virtual void Destroy() {}
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_ASYNC_RESPONSE_READER_H
