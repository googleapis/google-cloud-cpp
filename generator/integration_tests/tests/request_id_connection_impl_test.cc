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
#include "generator/integration_tests/tests/mock_request_id_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_testing::MockRequestIdServiceStub;
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
      "request has empty request_id",
      [](Request const& request) { return request.request_id(); }, IsEmpty());
}

auto IsUuidV4() {
  using ::testing::ContainsRegex;
  return ContainsRegex(
      "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
}

template <typename Request>
auto RequestIdIsUuidV4() {
  return ResultOf(
      "request has request_id in UUIDV4 format",
      [](Request const& request) { return request.request_id(); }, IsUuidV4());
}

auto MakeTestConnection(
    std::shared_ptr<golden_v1_internal::RequestIdServiceStub> stub) {
  auto options = golden_v1_internal::RequestIdServiceDefaultOptions({});
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<golden_v1_internal::RequestIdServiceConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

TEST(RequestIdTest, UnaryRpc) {
#if GTEST_USES_POSIX_RE == 0
  GTEST_SKIP();
#endif  // GTEST_USES_POSIX_RE
  std::vector<std::string> captured_ids;
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock, CreateFoo(_, _, RequestIdIsUuidV4<CreateFooRequest>()))
      .WillOnce([&](auto&, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        return StatusOr<Foo>(TransientError());
      })
      .WillOnce([&](auto&, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        return make_status_or(Foo{});
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  auto result = connection->CreateFoo(request);
  EXPECT_STATUS_OK(result);
  ASSERT_THAT(captured_ids, ElementsAre(IsUuidV4(), IsUuidV4()));
  EXPECT_EQ(captured_ids[0], captured_ids[1]);
}

TEST(RequestIdTest, UnaryRpcExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(
      *mock,
      CreateFoo(_, _, WithRequestId<CreateFooRequest>("test-request-id")))
      .WillOnce(Return(TransientError()))
      .WillOnce(Return(Foo{}));

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  request.set_request_id("test-request-id");
  auto result = connection->CreateFoo(request);
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, AsyncUnaryRpc) {
#if GTEST_USES_POSIX_RE == 0
  GTEST_SKIP();
#endif  // GTEST_USES_POSIX_RE
  std::vector<std::string> captured_ids;
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(*mock,
              AsyncCreateFoo(_, _, _, RequestIdIsUuidV4<CreateFooRequest>()))
      .WillOnce([&](auto, auto, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        return make_ready_future(StatusOr<Foo>(TransientError()));
      })
      .WillOnce([&](auto, auto, auto const&, auto const& request) {
        captured_ids.push_back(request.request_id());
        return make_ready_future(make_status_or(Foo{}));
      });

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  auto result = connection->AsyncCreateFoo(request).get();
  EXPECT_STATUS_OK(result);
  ASSERT_THAT(captured_ids, ElementsAre(IsUuidV4(), IsUuidV4()));
  EXPECT_EQ(captured_ids[0], captured_ids[1]);
}

TEST(RequestIdTest, AsyncUnaryRpcExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(
      *mock, AsyncCreateFoo(_, _, _,
                            WithRequestId<CreateFooRequest>("test-request-id")))
      .WillOnce(
          Return(ByMove(make_ready_future(StatusOr<Foo>(TransientError())))))
      .WillOnce(Return(ByMove(make_ready_future(make_status_or(Foo{})))));

  auto connection = MakeTestConnection(mock);
  internal::OptionsSpan span(connection->options());
  CreateFooRequest request;
  request.set_request_id("test-request-id");
  auto result = connection->AsyncCreateFoo(request).get();
  EXPECT_STATUS_OK(result);
}

TEST(RequestIdTest, Lro) {
#if GTEST_USES_POSIX_RE == 0
  GTEST_SKIP();
#endif  // GTEST_USES_POSIX_RE
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  std::vector<std::string> captured_ids;
  EXPECT_CALL(*mock,
              AsyncRenameFoo(_, _, _, RequestIdIsUuidV4<RenameFooRequest>()))
      .WillOnce([&](auto, auto, auto, auto const& request) {
        captured_ids.push_back(request.request_id());
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(TransientError()));
      })
      .WillOnce([&](auto, auto, auto, auto const& request) {
        captured_ids.push_back(request.request_id());
        return make_ready_future(
            make_status_or(google::longrunning::Operation{}));
      });
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
  ASSERT_THAT(captured_ids, ElementsAre(IsUuidV4(), IsUuidV4()));
  EXPECT_EQ(captured_ids[0], captured_ids[1]);
}

TEST(RequestIdTest, LroExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  EXPECT_CALL(
      *mock, AsyncRenameFoo(_, _, _,
                            WithRequestId<RenameFooRequest>("test-request-id")))
      .WillOnce(Return(ByMove(make_ready_future(
          StatusOr<google::longrunning::Operation>(TransientError())))))
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
  std::vector<std::string> sequence_ids;
  EXPECT_CALL(*mock, ListFoos(_, _, WithoutRequestId<ListFoosRequest>()))
      .WillOnce([&](auto&, auto const&, auto const& request) {
        sequence_ids.push_back(request.request_id());
        ListFoosResponse response;
        response.add_foos()->set_name("name-0");
        response.set_next_page_token("test-token-0");
        return response;
      })
      .WillOnce([&](auto&, auto const&, auto const& request) {
        sequence_ids.push_back(request.request_id());
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
  EXPECT_THAT(sequence_ids, ElementsAre(IsEmpty(), IsEmpty()));
}

TEST(RequestIdTest, PaginationExplicit) {
  auto mock = std::make_shared<MockRequestIdServiceStub>();
  std::vector<std::string> sequence_ids;
  EXPECT_CALL(*mock,
              ListFoos(_, _, WithRequestId<ListFoosRequest>("test-request-id")))
      .WillOnce([&](auto&, auto const&, auto const& request) {
        sequence_ids.push_back(request.request_id());
        ListFoosResponse response;
        response.add_foos()->set_name("name-0");
        response.set_next_page_token("test-token-0");
        return response;
      })
      .WillOnce([&](auto&, auto const&, auto const& request) {
        sequence_ids.push_back(request.request_id());
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
  EXPECT_THAT(sequence_ids, ElementsAre("test-request-id", "test-request-id"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
