// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/database_admin_connection.h"
#include "google/cloud/spanner/internal/polling_loop.h"
#include "google/cloud/spanner/internal/retry_loop.h"
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace gcsa = ::google::spanner::admin::database::v1;

namespace {

std::unique_ptr<RetryPolicy> DefaultAdminRetryPolicy() {
  return LimitedTimeRetryPolicy(std::chrono::minutes(30)).clone();
}

std::unique_ptr<BackoffPolicy> DefaultAdminBackoffPolicy() {
  auto constexpr kBackoffScaling = 2.0;
  return ExponentialBackoffPolicy(std::chrono::seconds(1),
                                  std::chrono::minutes(5), kBackoffScaling)
      .clone();
}

std::unique_ptr<PollingPolicy> DefaultAdminPollingPolicy() {
  auto constexpr kBackoffScaling = 2.0;
  return GenericPollingPolicy<>(
             LimitedTimeRetryPolicy(std::chrono::minutes(30)),
             ExponentialBackoffPolicy(std::chrono::seconds(10),
                                      std::chrono::minutes(5), kBackoffScaling))
      .clone();
}

class DatabaseAdminConnectionImpl : public DatabaseAdminConnection {
 public:
  explicit DatabaseAdminConnectionImpl(
      std::shared_ptr<internal::DatabaseAdminStub> stub,
      std::unique_ptr<RetryPolicy> retry_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy,
      std::unique_ptr<PollingPolicy> polling_policy)
      : stub_(std::move(stub)),
        retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        polling_policy_prototype_(std::move(polling_policy)) {}

  explicit DatabaseAdminConnectionImpl(
      std::shared_ptr<internal::DatabaseAdminStub> stub)
      : DatabaseAdminConnectionImpl(std::move(stub), DefaultAdminRetryPolicy(),
                                    DefaultAdminBackoffPolicy(),
                                    DefaultAdminPollingPolicy()) {}

  ~DatabaseAdminConnectionImpl() override = default;

  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(CreateDatabaseParams p) override {
    gcsa::CreateDatabaseRequest request;
    request.set_parent(p.database.instance().FullName());
    request.set_create_statement("CREATE DATABASE `" +
                                 p.database.database_id() + "`");
    for (auto& s : p.extra_statements) {
      *request.add_extra_statements() = std::move(s);
    }

    auto operation = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        false,
        [this](grpc::ClientContext& context,
               gcsa::CreateDatabaseRequest const& request) {
          return stub_->CreateDatabase(context, request);
        },
        request, __func__);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::Database>(operation.status()));
    }

    return AwaitCreateDatabase(*std::move(operation));
  }

  StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      GetDatabaseParams p) override {
    gcsa::GetDatabaseRequest request;
    request.set_name(p.database.FullName());
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        true,
        [this](grpc::ClientContext& context,
               gcsa::GetDatabaseRequest const& request) {
          return stub_->GetDatabase(context, request);
        },
        request, __func__);
  }

  StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>
  GetDatabaseDdl(GetDatabaseDdlParams p) override {
    gcsa::GetDatabaseDdlRequest request;
    request.set_database(p.database.FullName());
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        true,
        [this](grpc::ClientContext& context,
               gcsa::GetDatabaseDdlRequest const& request) {
          return stub_->GetDatabaseDdl(context, request);
        },
        request, __func__);
  }

  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
  UpdateDatabase(UpdateDatabaseParams p) override {
    gcsa::UpdateDatabaseDdlRequest request;
    request.set_database(p.database.FullName());
    for (auto& s : p.statements) {
      *request.add_statements() = std::move(s);
    }
    auto operation = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        false,
        [this](grpc::ClientContext& context,
               gcsa::UpdateDatabaseDdlRequest const& request) {
          return stub_->UpdateDatabase(context, request);
        },
        request, __func__);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::UpdateDatabaseDdlMetadata>(operation.status()));
    }

    return AwaitUpdateDatabase(*std::move(operation));
  }

  Status DropDatabase(DropDatabaseParams p) override {
    google::spanner::admin::database::v1::DropDatabaseRequest request;
    request.set_database(p.database.FullName());
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        true,
        [this](grpc::ClientContext& context,
               gcsa::DropDatabaseRequest const& request) {
          return stub_->DropDatabase(context, request);
        },
        request, __func__);
  }

  ListDatabaseRange ListDatabases(ListDatabasesParams p) override {
    gcsa::ListDatabasesRequest request;
    request.set_parent(p.instance.FullName());
    request.clear_page_token();
    auto stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<RetryPolicy const>(retry_policy_prototype_->clone());
    auto backoff = std::shared_ptr<BackoffPolicy const>(
        backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return ListDatabaseRange(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListDatabasesRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), true,
              [stub](grpc::ClientContext& context,
                     gcsa::ListDatabasesRequest const& request) {
                return stub->ListDatabases(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListDatabasesResponse r) {
          std::vector<gcsa::Database> result(r.databases().size());
          auto& dbs = *r.mutable_databases();
          std::move(dbs.begin(), dbs.end(), result.begin());
          return result;
        });
  }

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      GetIamPolicyParams p) override {
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(p.database.FullName());
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        true,
        [this](grpc::ClientContext& context,
               google::iam::v1::GetIamPolicyRequest const& request) {
          return stub_->GetIamPolicy(context, request);
        },
        request, __func__);
  }

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      SetIamPolicyParams p) override {
    google::iam::v1::SetIamPolicyRequest request;
    request.set_resource(p.database.FullName());
    *request.mutable_policy() = std::move(p.policy);
    bool is_idempotent = !request.policy().etag().empty();
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        is_idempotent,
        [this](grpc::ClientContext& context,
               google::iam::v1::SetIamPolicyRequest const& request) {
          return stub_->SetIamPolicy(context, request);
        },
        request, __func__);
  }

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      TestIamPermissionsParams p) override {
    google::iam::v1::TestIamPermissionsRequest request;
    request.set_resource(p.database.FullName());
    for (auto& permission : p.permissions) {
      request.add_permissions(std::move(permission));
    }
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        true,
        [this](grpc::ClientContext& context,
               google::iam::v1::TestIamPermissionsRequest const& request) {
          return stub_->TestIamPermissions(context, request);
        },
        request, __func__);
  }

 private:
  future<StatusOr<gcsa::Database>> AwaitCreateDatabase(
      google::longrunning::Operation operation) {
    promise<StatusOr<gcsa::Database>> pr;
    auto f = pr.get_future();

    // TODO(#127) - use the (implicit) completion queue to run this loop.
    std::thread t(
        [](std::shared_ptr<internal::DatabaseAdminStub> stub,
           google::longrunning::Operation operation,
           std::unique_ptr<PollingPolicy> polling_policy,
           google::cloud::promise<StatusOr<gcsa::Database>> promise,
           char const* location) mutable {
          auto result = internal::PollingLoop<
              internal::PollingLoopResponseExtractor<gcsa::Database>>(
              std::move(polling_policy),
              [stub](grpc::ClientContext& context,
                     google::longrunning::GetOperationRequest const& request) {
                return stub->GetOperation(context, request);
              },
              std::move(operation), location);

          // Drop our reference to stub; ideally we'd have std::moved into the
          // lambda. Doing this also prevents a false leak from being reported
          // when using googlemock.
          stub.reset();
          promise.set_value(std::move(result));
        },
        stub_, std::move(operation), polling_policy_prototype_->clone(),
        std::move(pr), __func__);
    t.detach();

    return f;
  }

  future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> AwaitUpdateDatabase(
      google::longrunning::Operation operation) {
    promise<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> pr;
    auto f = pr.get_future();

    // TODO(#127) - use the (implicit) completion queue to run this loop.
    std::thread t(
        [](std::shared_ptr<internal::DatabaseAdminStub> stub,
           google::longrunning::Operation operation,
           std::unique_ptr<PollingPolicy> polling_policy,
           promise<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> promise,
           char const* location) mutable {
          auto result =
              internal::PollingLoop<internal::PollingLoopMetadataExtractor<
                  gcsa::UpdateDatabaseDdlMetadata>>(
                  std::move(polling_policy),
                  [stub](
                      grpc::ClientContext& context,
                      google::longrunning::GetOperationRequest const& request) {
                    return stub->GetOperation(context, request);
                  },
                  std::move(operation), location);

          // Drop our reference to stub; ideally we'd have std::moved into the
          // lambda. Doing this also prevents a false leak from being reported
          // when using googlemock.
          stub.reset();
          promise.set_value(std::move(result));
        },
        stub_, std::move(operation), polling_policy_prototype_->clone(),
        std::move(pr), __func__);
    t.detach();

    return f;
  }

  std::shared_ptr<internal::DatabaseAdminStub> stub_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  std::unique_ptr<PollingPolicy const> polling_policy_prototype_;
};
}  // namespace

DatabaseAdminConnection::~DatabaseAdminConnection() = default;

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options) {
  return std::make_shared<DatabaseAdminConnectionImpl>(
      internal::CreateDefaultDatabaseAdminStub(options));
}

namespace internal {

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<internal::DatabaseAdminStub> stub,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy) {
  return std::make_shared<DatabaseAdminConnectionImpl>(
      std::move(stub), std::move(retry_policy), std::move(backoff_policy),
      std::move(polling_policy));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
