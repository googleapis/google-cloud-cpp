// Copyright 2021 Google LLC
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

#include "google/cloud/common_options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/grpc_impersonate_service_account.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::bigtable::admin::v2::GetTableRequest;
using ::google::bigtable::admin::v2::Table;
using ::google::cloud::MakeImpersonateServiceAccountCredentials;
using ::google::cloud::internal::ImpersonateServiceAccountConfig;
using ::google::cloud::internal::LogWrapper;
using ::testing::IsNull;
using ::testing::Not;

class GrpcImpersonateServiceAccountIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    bigtable_instance_id_ = google::cloud::internal::GetEnv(
                                "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                                .value_or("");
    iam_service_account_ = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT")
                               .value_or("");

    ASSERT_FALSE(project_id_.empty());
    ASSERT_FALSE(bigtable_instance_id_.empty());
    ASSERT_FALSE(iam_service_account_.empty());
  }

  std::string const& project_id() const { return project_id_; }
  std::string const& bigtable_instance_id() const {
    return bigtable_instance_id_;
  }
  std::string const& iam_service_account() const {
    return iam_service_account_;
  }

 private:
  std::string project_id_;
  std::string bigtable_instance_id_;
  std::string iam_service_account_;
};

class TestStub {
 public:
  virtual ~TestStub() = default;

  virtual StatusOr<Table> GetTable(grpc::ClientContext& context,
                                   GetTableRequest const& request) = 0;

  virtual future<StatusOr<Table>> AsyncGetTable(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      GetTableRequest const& request) = 0;
};

class TestStubImpl : public TestStub {
 public:
  explicit TestStubImpl(
      std::unique_ptr<google::bigtable::admin::v2::BigtableTableAdmin::Stub>
          stub)
      : stub_(std::move(stub)) {}

  StatusOr<Table> GetTable(grpc::ClientContext& context,
                           GetTableRequest const& request) override {
    Table response;
    auto grpc = stub_->GetTable(&context, request, &response);
    if (!grpc.ok()) return MakeStatusFromRpcError(grpc);
    return response;
  }

  future<StatusOr<Table>> AsyncGetTable(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      GetTableRequest const& request) override {
    return cq.MakeUnaryRpc(
        [this](grpc::ClientContext* context, GetTableRequest const& request,
               grpc::CompletionQueue* cq) {
          return stub_->AsyncGetTable(context, request, cq);
        },
        request, std::move(context));
  }

 private:
  std::unique_ptr<
      google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>
      stub_;
};

class TestStubAuth : public TestStub,
                     public std::enable_shared_from_this<TestStubAuth> {
 public:
  explicit TestStubAuth(std::shared_ptr<TestStub> child,
                        std::shared_ptr<GrpcImpersonateServiceAccount> auth)
      : child_(std::move(child)), auth_(std::move(auth)) {}

  StatusOr<Table> GetTable(grpc::ClientContext& context,
                           GetTableRequest const& request) override {
    auto s = auth_->ConfigureContext(context);
    if (!s.ok()) return s;
    return child_->GetTable(context, request);
  }

  future<StatusOr<Table>> AsyncGetTable(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      GetTableRequest const& request) override {
    auto self = shared_from_this();
    return auth_->AsyncConfigureContext(std::move(context))
        .then([self, cq,
               request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                            f) mutable {
          auto context = f.get();
          if (!context)
            return make_ready_future<StatusOr<Table>>(
                std::move(context).status());
          return self->child_->AsyncGetTable(cq, *std::move(context), request);
        });
  }

 private:
  std::shared_ptr<TestStub> child_;
  std::shared_ptr<GrpcImpersonateServiceAccount> auth_;
};

// This is not strictly needed for the test, but it made troubleshooting much
// easier.
class TestStubLogging : public TestStub {
 public:
  explicit TestStubLogging(std::shared_ptr<TestStub> child)
      : child_(std::move(child)) {}

  StatusOr<Table> GetTable(grpc::ClientContext& context,
                           GetTableRequest const& request) override {
    return LogWrapper(
        [this](grpc::ClientContext& context, GetTableRequest const& request) {
          return child_->GetTable(context, request);
        },
        context, request, __func__, TracingOptions{});
  }

  future<StatusOr<Table>> AsyncGetTable(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      GetTableRequest const& request) override {
    return LogWrapper(
        [this](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               GetTableRequest const& request) {
          return child_->AsyncGetTable(cq, std::move(context), request);
        },
        cq, std::move(context), request, __func__, TracingOptions{});
  }

 private:
  std::shared_ptr<TestStub> child_;
};

std::shared_ptr<TestStub> MakeTestStub(
    std::shared_ptr<grpc::Channel> channel,
    std::shared_ptr<GrpcImpersonateServiceAccount> auth) {
  std::shared_ptr<TestStub> stub = std::make_shared<TestStubImpl>(
      google::bigtable::admin::v2::BigtableTableAdmin::NewStub(
          std::move(channel)));
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<TestStubAuth>(std::move(stub), std::move(auth));
  }
  return std::make_shared<TestStubLogging>(std::move(stub));
}

TEST_F(GrpcImpersonateServiceAccountIntegrationTest, BlockingCallWithToken) {
  AutomaticallyCreatedBackgroundThreads background;
  auto credentials = MakeImpersonateServiceAccountCredentials(
      MakeGoogleDefaultCredentials(), iam_service_account(), Options{});
  auto* config =
      dynamic_cast<ImpersonateServiceAccountConfig*>(credentials.get());
  ASSERT_THAT(config, Not(IsNull()));
  auto under_test = GrpcImpersonateServiceAccount::Create(
      background.cq(), *config,
      Options{}.set<TracingComponentsOption>({"rpc"}));

  auto channel = under_test->CreateChannel("bigtableadmin.googleapis.com",
                                           grpc::ChannelArguments{});

  auto stub = MakeTestStub(std::move(channel), std::move(under_test));

  auto get_table = [&]() -> StatusOr<Table> {
    grpc::ClientContext context;
    google::bigtable::admin::v2::GetTableRequest request;
    request.set_name("projects/" + project_id() + "/instances/" +
                     bigtable_instance_id() + "/tables/quickstart");
    return stub->GetTable(context, request);
  };

  auto backoff_period = std::chrono::milliseconds(100);
  auto backoff = [&backoff_period] {
    std::this_thread::sleep_for(backoff_period);
    backoff_period *= 2;
  };
  for (int i = 0; i != 3; ++i) {
    auto result = get_table();
    if (result || result.status().code() == StatusCode::kNotFound) {
      SUCCEED();
      return;
    }
    backoff();
  }
  FAIL();
}

TEST_F(GrpcImpersonateServiceAccountIntegrationTest, AsyncCallWithToken) {
  AutomaticallyCreatedBackgroundThreads background;
  auto credentials = MakeImpersonateServiceAccountCredentials(
      MakeGoogleDefaultCredentials(), iam_service_account(), Options{});
  auto* config =
      dynamic_cast<ImpersonateServiceAccountConfig*>(credentials.get());
  ASSERT_THAT(config, Not(IsNull()));
  auto under_test = GrpcImpersonateServiceAccount::Create(
      background.cq(), *config,
      Options{}.set<TracingComponentsOption>({"rpc"}));

  auto channel = under_test->CreateChannel("bigtableadmin.googleapis.com",
                                           grpc::ChannelArguments{});

  auto stub = MakeTestStub(std::move(channel), std::move(under_test));

  auto async_get_table =
      [&](google::cloud::CompletionQueue& cq,
          GetTableRequest const& request) -> future<StatusOr<Table>> {
    return stub->AsyncGetTable(cq, absl::make_unique<grpc::ClientContext>(),
                               request);
  };

  auto backoff_period = std::chrono::milliseconds(100);
  auto backoff = [&backoff_period] {
    std::this_thread::sleep_for(backoff_period);
    backoff_period *= 2;
  };
  for (int i = 0; i != 3; ++i) {
    auto cq = background.cq();
    GetTableRequest request;
    request.set_name("projects/" + project_id() + "/instances/" +
                     bigtable_instance_id() + "/tables/quickstart");
    auto result = async_get_table(cq, request).get();
    if (result || result.status().code() == StatusCode::kNotFound) {
      SUCCEED();
      return;
    }
    backoff();
  }
  FAIL();
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
