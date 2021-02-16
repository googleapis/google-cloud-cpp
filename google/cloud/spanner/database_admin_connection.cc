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
#include "google/cloud/internal/polling_loop.h"
#include "google/cloud/internal/retry_loop.h"
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = ::google::spanner::admin::database::v1;

using google::cloud::internal::Idempotency;
using google::cloud::internal::PollingLoop;
using google::cloud::internal::PollingLoopMetadataExtractor;
using google::cloud::internal::PollingLoopResponseExtractor;

future<StatusOr<google::spanner::admin::database::v1::Backup>>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
DatabaseAdminConnection::CreateBackup(CreateBackupParams) {
  return google::cloud::make_ready_future(StatusOr<gcsa::Backup>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

future<StatusOr<google::spanner::admin::database::v1::Database>>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
DatabaseAdminConnection::RestoreDatabase(RestoreDatabaseParams) {
  return google::cloud::make_ready_future(StatusOr<gcsa::Database>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

StatusOr<google::spanner::admin::database::v1::Backup>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
DatabaseAdminConnection::GetBackup(GetBackupParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status DatabaseAdminConnection::DeleteBackup(DeleteBackupParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
ListBackupsRange DatabaseAdminConnection::ListBackups(ListBackupsParams) {
  return google::cloud::internal::MakePaginationRange<ListBackupsRange>(
      gcsa::ListBackupsRequest{},
      [](gcsa::ListBackupsRequest const&) {
        return StatusOr<gcsa::ListBackupsResponse>(
            Status(StatusCode::kUnimplemented, "not implemented"));
      },
      [](gcsa::ListBackupsResponse const&) {
        return std::vector<gcsa::Backup>{};
      });
}

StatusOr<google::spanner::admin::database::v1::Backup>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
DatabaseAdminConnection::UpdateBackup(UpdateBackupParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

ListBackupOperationsRange DatabaseAdminConnection::ListBackupOperations(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    ListBackupOperationsParams) {
  return google::cloud::internal::MakePaginationRange<
      ListBackupOperationsRange>(
      gcsa::ListBackupOperationsRequest{},
      [](gcsa::ListBackupOperationsRequest const&) {
        return StatusOr<gcsa::ListBackupOperationsResponse>(
            Status(StatusCode::kUnimplemented, "not implemented"));
      },
      [](gcsa::ListBackupOperationsResponse const&) {
        return std::vector<google::longrunning::Operation>{};
      });
}

ListDatabaseOperationsRange DatabaseAdminConnection::ListDatabaseOperations(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    ListDatabaseOperationsParams) {
  return google::cloud::internal::MakePaginationRange<
      ListDatabaseOperationsRange>(
      gcsa::ListDatabaseOperationsRequest{},
      [](gcsa::ListDatabaseOperationsRequest const&) {
        return StatusOr<gcsa::ListDatabaseOperationsResponse>(
            Status(StatusCode::kUnimplemented, "not implemented"));
      },
      [](gcsa::ListDatabaseOperationsResponse const&) {
        return std::vector<google::longrunning::Operation>{};
      });
}

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
      ConnectionOptions const& options,
      std::unique_ptr<RetryPolicy> retry_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy,
      std::unique_ptr<PollingPolicy> polling_policy)
      : stub_(std::move(stub)),
        retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        polling_policy_prototype_(std::move(polling_policy)),
        background_threads_(options.background_threads_factory()()) {}

  explicit DatabaseAdminConnectionImpl(
      std::shared_ptr<internal::DatabaseAdminStub> stub,
      ConnectionOptions const& options)
      : DatabaseAdminConnectionImpl(
            std::move(stub), options, DefaultAdminRetryPolicy(),
            DefaultAdminBackoffPolicy(), DefaultAdminPollingPolicy()) {}

  ~DatabaseAdminConnectionImpl() override = default;

  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(CreateDatabaseParams p) override {
    gcsa::CreateDatabaseRequest request;
    request.set_parent(p.database.instance().FullName());
    request.set_create_statement("CREATE DATABASE `" +
                                 p.database.database_id() + "`");
    if (p.encryption_key) {
      request.mutable_encryption_config()->set_kms_key_name(
          p.encryption_key->FullName());
    }
    for (auto& s : p.extra_statements) {
      *request.add_extra_statements() = std::move(s);
    }

    auto operation = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::CreateDatabaseRequest const& request) {
          return stub_->CreateDatabase(context, request);
        },
        request, __func__);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::Database>(operation.status()));
    }

    return AwaitDatabase(*std::move(operation));
  }

  StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      GetDatabaseParams p) override {
    gcsa::GetDatabaseRequest request;
    request.set_name(p.database.FullName());
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
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
        Idempotency::kIdempotent,
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
        Idempotency::kNonIdempotent,
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
        Idempotency::kIdempotent,
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
    return google::cloud::internal::MakePaginationRange<ListDatabaseRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListDatabasesRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
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

  future<StatusOr<google::spanner::admin::database::v1::Database>>
  RestoreDatabase(RestoreDatabaseParams p) override {
    gcsa::RestoreDatabaseRequest request;
    request.set_parent(p.database.instance().FullName());
    request.set_database_id(p.database.database_id());
    request.set_backup(std::move(p.backup_full_name));
    if (p.encryption_key) {
      auto& encryption = *request.mutable_encryption_config();
      encryption.set_encryption_type(
          gcsa::RestoreDatabaseEncryptionConfig::CUSTOMER_MANAGED_ENCRYPTION);
      encryption.set_kms_key_name(p.encryption_key->FullName());
    }
    auto operation = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::RestoreDatabaseRequest const& request) {
          return stub_->RestoreDatabase(context, request);
        },
        request, __func__);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::Database>(operation.status()));
    }

    return AwaitDatabase(*std::move(operation));
  }

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      GetIamPolicyParams p) override {
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(p.database.FullName());
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
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
    auto const idempotency = request.policy().etag().empty()
                                 ? Idempotency::kNonIdempotent
                                 : Idempotency::kIdempotent;
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        idempotency,
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
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::iam::v1::TestIamPermissionsRequest const& request) {
          return stub_->TestIamPermissions(context, request);
        },
        request, __func__);
  }

  future<StatusOr<gcsa::Backup>> CreateBackup(CreateBackupParams p) override {
    gcsa::CreateBackupRequest request;
    request.set_parent(p.database.instance().FullName());
    request.set_backup_id(p.backup_id);
    auto& backup = *request.mutable_backup();
    backup.set_database(p.database.FullName());
    // `p.expire_time` is deprecated and ignored here.
    *backup.mutable_expire_time() =
        p.expire_timestamp.get<protobuf::Timestamp>().value();
    if (p.version_time) {
      *backup.mutable_version_time() =
          p.version_time->get<protobuf::Timestamp>().value();
    }
    if (p.encryption_key) {
      auto& encryption = *request.mutable_encryption_config();
      encryption.set_encryption_type(
          gcsa::CreateBackupEncryptionConfig::CUSTOMER_MANAGED_ENCRYPTION);
      encryption.set_kms_key_name(p.encryption_key->FullName());
    }
    auto operation = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::CreateBackupRequest const& request) {
          return stub_->CreateBackup(context, request);
        },
        request, __func__);
    if (!operation) {
      return google::cloud::make_ready_future(
          StatusOr<gcsa::Backup>(operation.status()));
    }

    return AwaitCreateBackup(*std::move(operation));
  }

  StatusOr<google::spanner::admin::database::v1::Backup> GetBackup(
      GetBackupParams p) override {
    gcsa::GetBackupRequest request;
    request.set_name(p.backup_full_name);
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::GetBackupRequest const& request) {
          return stub_->GetBackup(context, request);
        },
        request, __func__);
  }

  Status DeleteBackup(DeleteBackupParams p) override {
    google::spanner::admin::database::v1::DeleteBackupRequest request;
    request.set_name(p.backup_full_name);
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::DeleteBackupRequest const& request) {
          return stub_->DeleteBackup(context, request);
        },
        request, __func__);
  }

  ListBackupsRange ListBackups(ListBackupsParams p) override {
    gcsa::ListBackupsRequest request;
    request.set_parent(p.instance.FullName());
    request.set_filter(std::move(p.filter));
    auto stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<RetryPolicy const>(retry_policy_prototype_->clone());
    auto backoff = std::shared_ptr<BackoffPolicy const>(
        backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return google::cloud::internal::MakePaginationRange<ListBackupsRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListBackupsRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gcsa::ListBackupsRequest const& request) {
                return stub->ListBackups(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListBackupsResponse r) {
          std::vector<gcsa::Backup> result(r.backups().size());
          auto& backups = *r.mutable_backups();
          std::move(backups.begin(), backups.end(), result.begin());
          return result;
        });
  }

  StatusOr<google::spanner::admin::database::v1::Backup> UpdateBackup(
      UpdateBackupParams p) override {
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::UpdateBackupRequest const& request) {
          return stub_->UpdateBackup(context, request);
        },
        p.request, __func__);
  }

  ListBackupOperationsRange ListBackupOperations(
      ListBackupOperationsParams p) override {
    gcsa::ListBackupOperationsRequest request;
    request.set_parent(p.instance.FullName());
    request.set_filter(std::move(p.filter));
    auto stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<RetryPolicy const>(retry_policy_prototype_->clone());
    auto backoff = std::shared_ptr<BackoffPolicy const>(
        backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return google::cloud::internal::MakePaginationRange<
        ListBackupOperationsRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListBackupOperationsRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gcsa::ListBackupOperationsRequest const& request) {
                return stub->ListBackupOperations(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListBackupOperationsResponse r) {
          std::vector<google::longrunning::Operation> result(
              r.operations().size());
          auto& operations = *r.mutable_operations();
          std::move(operations.begin(), operations.end(), result.begin());
          return result;
        });
  }

  ListDatabaseOperationsRange ListDatabaseOperations(
      ListDatabaseOperationsParams p) override {
    gcsa::ListDatabaseOperationsRequest request;
    request.set_parent(p.instance.FullName());
    request.set_filter(std::move(p.filter));
    auto stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry =
        std::shared_ptr<RetryPolicy const>(retry_policy_prototype_->clone());
    auto backoff = std::shared_ptr<BackoffPolicy const>(
        backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return google::cloud::internal::MakePaginationRange<
        ListDatabaseOperationsRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListDatabaseOperationsRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gcsa::ListDatabaseOperationsRequest const& request) {
                return stub->ListDatabaseOperations(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListDatabaseOperationsResponse r) {
          std::vector<google::longrunning::Operation> result(
              r.operations().size());
          auto& operations = *r.mutable_operations();
          std::move(operations.begin(), operations.end(), result.begin());
          return result;
        });
  }

 private:
  future<StatusOr<gcsa::Database>> AwaitDatabase(
      google::longrunning::Operation operation) {
    promise<StatusOr<gcsa::Database>> pr;
    auto f = pr.get_future();

    // TODO(#4038) - use background_threads_->cq() to run this loop.
    std::thread t(
        [](std::shared_ptr<internal::DatabaseAdminStub> stub,
           google::longrunning::Operation operation,
           std::unique_ptr<PollingPolicy> polling_policy,
           google::cloud::promise<StatusOr<gcsa::Database>> promise,
           char const* location) mutable {
          auto result =
              PollingLoop<PollingLoopResponseExtractor<gcsa::Database>>(
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

  future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> AwaitUpdateDatabase(
      google::longrunning::Operation operation) {
    promise<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> pr;
    auto f = pr.get_future();

    // TODO(#4038) - use background_threads_->cq() to run this loop.
    std::thread t(
        [](std::shared_ptr<internal::DatabaseAdminStub> stub,
           google::longrunning::Operation operation,
           std::unique_ptr<PollingPolicy> polling_policy,
           promise<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> promise,
           char const* location) mutable {
          auto result = PollingLoop<
              PollingLoopMetadataExtractor<gcsa::UpdateDatabaseDdlMetadata>>(
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

  future<StatusOr<gcsa::Backup>> AwaitCreateBackup(
      google::longrunning::Operation operation) {
    // Create a local copy of stub because `this` might get out of scope when
    // the callback will be called.
    std::shared_ptr<internal::DatabaseAdminStub> cancel_stub(stub_);
    // Create a promise with a cancellation callback.
    promise<StatusOr<gcsa::Backup>> pr([cancel_stub, operation]() {
      grpc::ClientContext context;
      google::longrunning::CancelOperationRequest request;
      request.set_name(operation.name());
      cancel_stub->CancelOperation(context, request);
    });
    auto f = pr.get_future();

    // TODO(#4038) - use background_threads_->cq() to run this loop.
    std::thread t(
        [](std::shared_ptr<internal::DatabaseAdminStub> stub,
           google::longrunning::Operation operation,
           std::unique_ptr<PollingPolicy> polling_policy,
           google::cloud::promise<StatusOr<gcsa::Backup>> promise,
           char const* location) mutable {
          auto result = PollingLoop<PollingLoopResponseExtractor<gcsa::Backup>>(
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

  std::shared_ptr<internal::DatabaseAdminStub> stub_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  std::unique_ptr<PollingPolicy const> polling_policy_prototype_;

  // Implementations of `BackgroundThreads` typically create a pool of
  // threads that are joined during destruction, so, to avoid ownership
  // cycles, those threads should never assume ownership of this object
  // (e.g., via a `std::shared_ptr<>`).
  std::unique_ptr<BackgroundThreads> background_threads_;
};

}  // namespace

DatabaseAdminConnection::~DatabaseAdminConnection() = default;

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options) {
  return internal::MakeDatabaseAdminConnection(
      internal::CreateDefaultDatabaseAdminStub(options), options);
}

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy) {
  return internal::MakeDatabaseAdminConnection(
      internal::CreateDefaultDatabaseAdminStub(options), options,
      std::move(retry_policy), std::move(backoff_policy),
      std::move(polling_policy));
}

namespace internal {

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<internal::DatabaseAdminStub> stub,
    ConnectionOptions const& options) {
  return std::make_shared<DatabaseAdminConnectionImpl>(std::move(stub),
                                                       options);
}

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    std::shared_ptr<internal::DatabaseAdminStub> stub,
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy) {
  return std::make_shared<DatabaseAdminConnectionImpl>(
      std::move(stub), options, std::move(retry_policy),
      std::move(backoff_policy), std::move(polling_policy));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
