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

#include "google/cloud/pubsublite/internal/service_composite.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;

class MockService : public Service {
 public:
  MOCK_METHOD(future<Status>, Start, (), (override));
  MOCK_METHOD(future<void>, Shutdown, (), (override));
};

TEST(ServiceTest, SingleDependencyGood) {
  InSequence seq;

  MockService service;
  promise<Status> status_promise;
  ServiceComposite service_composite{service};
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();
  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  // Abort call can safely be called more than once and not change final status
  // if already set
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST(ServiceTest, SingleDependencyStartFailed) {
  InSequence seq;

  MockService service;
  promise<Status> status_promise;
  ServiceComposite service_composite{service};
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();
  status_promise.set_value(Status(StatusCode::kAborted, "uh oh"));
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "uh oh"));
  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  // Abort call can safely be called more than once and not change final status
  // if already set
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "uh oh"));
}

TEST(ServiceTest, SingleDependencyStartFinishedOk) {
  InSequence seq;

  MockService service;
  promise<Status> status_promise;
  ServiceComposite service_composite{service};
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();
  status_promise.set_value(Status(StatusCode::kOk, "all good"));
  // above shouldn't invoke `Abort`
  EXPECT_EQ(service_composite.status(), Status());
  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST(ServiceTest, SingleDependencyShutdownTwice) {
  InSequence seq;

  MockService service;
  promise<Status> status_promise;
  ServiceComposite service_composite{service};
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();
  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  // Abort call can safely be called more than once and not change final status
  // if already set
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
  // shouldn't do anything
  service_composite.Shutdown();
}

TEST(ServiceTest, MultipleDependencyGood) {
  InSequence seq;

  MockService service;
  MockService service1;
  MockService service2;

  promise<Status> status_promise;
  promise<Status> status_promise1;
  promise<Status> status_promise2;

  ServiceComposite service_composite{service, service1, service2};

  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  EXPECT_CALL(service1, Start)
      .WillOnce(Return(ByMove(status_promise1.get_future())));
  EXPECT_CALL(service2, Start)
      .WillOnce(Return(ByMove(status_promise2.get_future())));

  future<Status> service_composite_start = service_composite.Start();

  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service1, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service2, Shutdown).WillOnce(Return(ByMove(make_ready_future())));

  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));

  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  status_promise1.set_value(Status(StatusCode::kOk, "test ok"));
  status_promise2.set_value(Status(StatusCode::kOk, "test ok"));

  // Abort call can safely be called more than once and not change final status
  // if already set
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST(ServiceTest, MultipleDependencySingleStartFailed) {
  InSequence seq;

  MockService service;
  MockService service1;
  MockService service2;

  promise<Status> status_promise;
  promise<Status> status_promise1;
  promise<Status> status_promise2;

  ServiceComposite service_composite{service, service1, service2};

  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  EXPECT_CALL(service1, Start)
      .WillOnce(Return(ByMove(status_promise1.get_future())));
  EXPECT_CALL(service2, Start)
      .WillOnce(Return(ByMove(status_promise2.get_future())));

  future<Status> service_composite_start = service_composite.Start();

  status_promise1.set_value(Status(StatusCode::kAborted, "oops"));
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "oops"));

  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service1, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service2, Shutdown).WillOnce(Return(ByMove(make_ready_future())));

  service_composite.Shutdown();

  // should not change final status b/c already set
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "oops"));

  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  status_promise2.set_value(Status(StatusCode::kOk, "test ok"));

  // Abort call can safely be called more than once and should not change final
  // status if already set
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "oops"));
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
