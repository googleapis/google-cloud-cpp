// Copyright 2024 Google LLC
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

#include "generator/integration_tests/golden/v1/internal/request_id_connection_impl.h"
#include "generator/integration_tests/golden/v1/internal/request_id_option_defaults.h"
#include "generator/integration_tests/golden/v1/internal/request_id_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::test::requestid::v1::CreateFooRequest;
using ::google::test::requestid::v1::Foo;
using ::google::test::requestid::v1::ListFoosRequest;
using ::google::test::requestid::v1::ListFoosResponse;
using ::google::test::requestid::v1::RenameFooRequest;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::ResultOf;
using ::testing::Return;

class MockRequestIdServiceStub
    : public google::cloud::golden_v1_internal::RequestIdServiceStub {
 public:
  ~MockRequestIdServiceStub() override = default;

  MOCK_METHOD(StatusOr<google::test::requestid::v1::Foo>, CreateFoo,
              (grpc::ClientContext&,
               google::test::requestid::v1::CreateFooRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>, AsyncRenameFoo,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>, Options const&,
               google::test::requestid::v1::RenameFooRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::test::requestid::v1::ListFoosResponse>, ListFoos,
              (grpc::ClientContext&,
               google::test::requestid::v1::ListFoosRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::test::requestid::v1::Foo>>,
              AsyncCreateFoo,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::test::requestid::v1::CreateFooRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>, Options const&,
               google::longrunning::GetOperationRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>, Options const&,
               google::longrunning::CancelOperationRequest const&),
              (override));
};

template <typename Request>
auto WithRequestId(std::string expected) {
  return ResultOf(
      "request has request_id",
      [](Request const& request) { return request.request_id(); },
      Eq(std::move(expected)));
}

template <typename Request>
auto WithoutRequestId() {
  return ResultOf(
      "request does not have request_id",
      [](Request const& request) { return request.request_id(); }, IsEmpty());
}

auto MakeTestConnection(
    std::shared_ptr<golden_v1_internal::RequestIdServiceStub> stub) {
  auto options = golden_v1_internal::RequestIdServiceDefaultOptions({});
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<golden_v1_internal::RequestIdServiceConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

TEST(RequestIdTest, UnaryRpc) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock, CreateFoo(_, WithoutRequestId<CreateFooRequest>()))
      .WillOnce(Return(Foo{}));

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  auto result = connection->CreateFoo(request);
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, UnaryRpcExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock,
              CreateFoo(_, WithRequestId<CreateFooRequest>("test-request-id")))
      .WillOnce(Return(Foo{}));

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  request.set_request_id("test-request-id");
  auto result = connection->CreateFoo(request);
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, AsyncUnaryRpc) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock, AsyncCreateFoo(_, _, WithoutRequestId<CreateFooRequest>()))
      .WillOnce(Return(ByMove(make_ready_future(make_status_or(Foo{})))));

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  auto result = connection->AsyncCreateFoo(request).get();
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, AsyncUnaryRpcExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(
      *mock,
      AsyncCreateFoo(_, _, WithRequestId<CreateFooRequest>("test-request-id")))
      .WillOnce(Return(ByMove(make_ready_future(make_status_or(Foo{})))));

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  request.set_request_id("test-request-id");
  auto result = connection->AsyncCreateFoo(request).get();
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, Lro) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock,
              AsyncRenameFoo(_, _, _, WithoutRequestId<RenameFooRequest>()))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(google::longrunning::Operation{})))));
  EXPECT_CALL(*mock, AsyncGetOperation).WillOnce([] {
    google::longrunning::Operation result;
    result.set_done(true);
    result.mutable_response()->PackFrom(Foo{});
    return make_ready_future(make_status_or(std::move(result)));
  });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  RenameFooRequest request;
  auto result = connection->RenameFoo(request).get();
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, LroExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(
      *mock, AsyncRenameFoo(_, _, _,
                            WithRequestId<RenameFooRequest>("test-request-id")))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(google::longrunning::Operation{})))));
  EXPECT_CALL(*mock, AsyncGetOperation).WillOnce([] {
    google::longrunning::Operation result;
    result.set_done(true);
    result.mutable_response()->PackFrom(Foo{});
    return make_ready_future(make_status_or(std::move(result)));
  });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  RenameFooRequest request;
  request.set_request_id("test-request-id");
  auto result = connection->RenameFoo(request).get();
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, Pagination) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  std::set<std::string> sequence_ids;
  EXPECT_CALL(*mock, ListFoos(_, WithoutRequestId<ListFoosRequest>()))
      .WillOnce([&](auto&, auto const& request) {
        sequence_ids.insert(request.request_id());
        ListFoosResponse response;
        response.add_foos()->set_name("name-0");
        response.set_next_page_token("test-token-0");
        return response;
      })
      .WillOnce([&](auto&, auto const& request) {
        sequence_ids.insert(request.request_id());
        ListFoosResponse response;
        response.add_foos()->set_name("name-1");
        return response;
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  ListFoosRequest request;
  auto range = connection->ListFoos(request);
  std::vector<StatusOr<Foo>> results{range.begin(), range.end()};
  auto with_name = [](std::string name) {
    return ResultOf(
        "Foo name is", [](Foo const& foo) { return foo.name(); },
        Eq(std::move(name)));
  };
  EXPECT_THAT(results, ElementsAre(IsOkAndHolds(with_name("name-0")),
                                   IsOkAndHolds(with_name("name-1"))));
  EXPECT_THAT(sequence_ids, ElementsAre(IsEmpty()));
}

TEST(RequestIdTest, PaginationExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  std::set<std::string> sequence_ids;
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock,
              ListFoos(_, WithRequestId<ListFoosRequest>("test-request-id")))
      .WillOnce([&](auto&, auto const& request) {
        sequence_ids.insert(request.request_id());
        ListFoosResponse response;
        response.add_foos()->set_name("name-0");
        response.set_next_page_token("test-token-0");
        return response;
      });
  EXPECT_CALL(*mock,
              ListFoos(_, WithRequestId<ListFoosRequest>("test-request-id")))
      .WillOnce([&](auto&, auto const& request) {
        sequence_ids.insert(request.request_id());
        ListFoosResponse response;
        response.add_foos()->set_name("name-1");
        return response;
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  ListFoosRequest request;
  request.set_request_id("test-request-id");
  auto range = connection->ListFoos(request);
  std::vector<StatusOr<Foo>> results{range.begin(), range.end()};
  auto with_name = [](std::string name) {
    return ResultOf(
        "Foo name is", [](Foo const& foo) { return foo.name(); },
        Eq(std::move(name)));
  };
  EXPECT_THAT(results, ElementsAre(IsOkAndHolds(with_name("name-0")),
                                   IsOkAndHolds(with_name("name-1"))));
  EXPECT_THAT(sequence_ids, ElementsAre("test-request-id"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
