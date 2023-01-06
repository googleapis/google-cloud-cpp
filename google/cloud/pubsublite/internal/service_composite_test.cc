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
#include "google/cloud/pubsublite/testing/mock_service.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

using ::google::cloud::pubsublite_testing::MockService;

TEST(ServiceTest, SingleDependencyNoStartDestructorGood) {
  StrictMock<MockService> service;
  ServiceComposite service_composite{&service};
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kFailedPrecondition, "`Start` not called"));
}

TEST(ServiceTest, SingleDependencyGood) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};
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
  EXPECT_EQ(service_composite_start.get(), Status());
}

TEST(ServiceTest, SingleDependencyStartFailed) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};
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
  EXPECT_EQ(service_composite_start.get(),
            Status(StatusCode::kAborted, "uh oh"));
}

TEST(ServiceTest, SingleDependencyStartFinishedOk) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};
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
  EXPECT_EQ(service_composite_start.get(), Status());
}

TEST(ServiceTest, SingleDependencyShutdownTwice) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};
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
  EXPECT_EQ(service_composite_start.get(), Status());
  // shouldn't do anything
  service_composite.Shutdown();
}

TEST(ServiceTest, MultipleDependencyGood) {
  InSequence seq;

  StrictMock<MockService> service;
  StrictMock<MockService> service1;
  StrictMock<MockService> service2;

  promise<Status> status_promise;
  promise<Status> status_promise1;
  promise<Status> status_promise2;

  ServiceComposite service_composite{&service, &service1, &service2};

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

  EXPECT_EQ(service_composite_start.get(), Status());
}

TEST(ServiceTest, MultipleDependencySingleStartFailed) {
  InSequence seq;

  StrictMock<MockService> service;
  StrictMock<MockService> service1;
  StrictMock<MockService> service2;

  promise<Status> status_promise;
  promise<Status> status_promise1;
  promise<Status> status_promise2;

  ServiceComposite service_composite{&service, &service1, &service2};

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

  EXPECT_EQ(service_composite_start.get(),
            Status(StatusCode::kAborted, "oops"));
}

TEST(ServiceTest, AddSingleDependencyToEmptyObjectGood) {
  InSequence seq;

  ServiceComposite service_composite;
  future<Status> service_composite_start = service_composite.Start();

  StrictMock<MockService> service;
  promise<Status> status_promise;
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  service_composite.AddServiceObject(&service);

  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  // Abort call can safely be called more than once and not change final status
  // if already set
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));

  EXPECT_EQ(service_composite_start.get(), Status());
}

TEST(ServiceTest, AddSingleDependencyToEmptyObjectStartFailed) {
  InSequence seq;

  ServiceComposite service_composite;
  future<Status> service_composite_start = service_composite.Start();

  StrictMock<MockService> service;
  promise<Status> status_promise;
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  service_composite.AddServiceObject(&service);

  status_promise.set_value(Status(StatusCode::kAborted, "oh no"));
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "oh no"));

  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();

  EXPECT_EQ(service_composite_start.get(),
            Status(StatusCode::kAborted, "oh no"));
}

TEST(ServiceTest, AddSingleDependencyToNonEmptyObjectGood) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};

  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();

  StrictMock<MockService> service1;
  promise<Status> status_promise1;
  EXPECT_CALL(service1, Start)
      .WillOnce(Return(ByMove(status_promise1.get_future())));
  service_composite.AddServiceObject(&service1);

  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service1, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  status_promise1.set_value(Status(StatusCode::kOk, "test ok"));

  EXPECT_EQ(service_composite_start.get(), Status());
}

TEST(ServiceTest, AddSingleDependencyToNonEmptyObjectStartFailed) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};

  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();

  StrictMock<MockService> service1;
  promise<Status> status_promise1;
  EXPECT_CALL(service1, Start)
      .WillOnce(Return(ByMove(status_promise1.get_future())));
  service_composite.AddServiceObject(&service1);

  status_promise1.set_value(Status(StatusCode::kAborted, "not ok"));
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "not ok"));

  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service1, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  status_promise.set_value(Status(StatusCode::kOk, "test ok"));

  EXPECT_EQ(service_composite_start.get(),
            Status(StatusCode::kAborted, "not ok"));
}

TEST(ServiceTest, AddDependencyAfterShutdown) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();
  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  EXPECT_EQ(service_composite.status(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
  status_promise.set_value(Status(StatusCode::kOk, "test ok"));
  StrictMock<MockService> service1;
  service_composite.AddServiceObject(&service1);
}

TEST(ServiceTest, AddDependencyAfterStartFailedBeforeShutdown) {
  InSequence seq;

  StrictMock<MockService> service;
  promise<Status> status_promise;
  ServiceComposite service_composite{&service};
  EXPECT_CALL(service, Start)
      .WillOnce(Return(ByMove(status_promise.get_future())));
  future<Status> service_composite_start = service_composite.Start();
  status_promise.set_value(Status(StatusCode::kAborted, "abort"));
  EXPECT_EQ(service_composite.status(), Status(StatusCode::kAborted, "abort"));
  StrictMock<MockService> service1;
  promise<Status> status_promise1;
  EXPECT_CALL(service1, Start)
      .WillOnce(Return(ByMove(status_promise1.get_future())));
  service_composite.AddServiceObject(&service1);
  EXPECT_CALL(service, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  EXPECT_CALL(service1, Shutdown).WillOnce(Return(ByMove(make_ready_future())));
  service_composite.Shutdown();
  status_promise1.set_value(Status());

  EXPECT_EQ(service_composite_start.get(),
            Status(StatusCode::kAborted, "abort"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
