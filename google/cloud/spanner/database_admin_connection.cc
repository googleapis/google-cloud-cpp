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
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/async_long_running_operation.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/options.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = ::google::spanner::admin::database::v1;

using ::google::cloud::internal::Idempotency;
using ::google::cloud::internal::RetryLoop;

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

class DatabaseAdminConnectionImpl : public DatabaseAdminConnection {
 public:
  // Note all the policies will be set to their default non-null values in the
  // `MakeDatabaseAdminConnection()` function below.
  explicit DatabaseAdminConnectionImpl(
      std::shared_ptr<spanner_internal::DatabaseAdminStub> stub,
      Options const& opts)
      : stub_(std::move(stub)),
        retry_policy_prototype_(opts.get<SpannerRetryPolicyOption>()->clone()),
        backoff_policy_prototype_(
            opts.get<SpannerBackoffPolicyOption>()->clone()),
        polling_policy_prototype_(
            opts.get<SpannerPollingPolicyOption>()->clone()),
        background_threads_(internal::MakeBackgroundThreadsFactory(opts)()) {}

  ~DatabaseAdminConnectionImpl() override = default;

  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(CreateDatabaseParams p) override {
    gcsa::CreateDatabaseRequest request;
    request.set_parent(p.database.instance().FullName());
    request.set_create_statement("CREATE DATABASE `" +
                                 p.database.database_id() + "`");
    struct EncryptionVisitor {
      explicit EncryptionVisitor(gcsa::CreateDatabaseRequest& request)
          : request_(request) {}
      void operator()(DefaultEncryption const&) const {
        // No encryption_config => GOOGLE_DEFAULT_ENCRYPTION.
      }
      void operator()(GoogleEncryption const&) const {
        // No encryption_config => GOOGLE_DEFAULT_ENCRYPTION.
      }
      void operator()(CustomerManagedEncryption const& cme) const {
        auto* config = request_.mutable_encryption_config();
        config->set_kms_key_name(cme.encryption_key().FullName());
      }
      gcsa::CreateDatabaseRequest& request_;
    };
    absl::visit(EncryptionVisitor(request), p.encryption_config);
    for (auto& s : p.extra_statements) {
      *request.add_extra_statements() = std::move(s);
    }
    auto stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<gcsa::Database>(
        background_threads_->cq(), std::move(request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gcsa::CreateDatabaseRequest const& request) {
          return stub->AsyncCreateDatabase(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::GetOperationRequest const& request) {
          return stub->AsyncGetOperation(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::CancelOperationRequest const& request) {
          return stub->AsyncCancelOperation(cq, std::move(context), request);
        },
        &google::cloud::internal::ExtractLongRunningResultResponse<
            gcsa::Database>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent, polling_policy_prototype_->clone(),
        __func__);
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
    auto stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<
        gcsa::UpdateDatabaseDdlMetadata>(
        background_threads_->cq(), std::move(request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gcsa::UpdateDatabaseDdlRequest const& request) {
          return stub->AsyncUpdateDatabaseDdl(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::GetOperationRequest const& request) {
          return stub->AsyncGetOperation(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::CancelOperationRequest const& request) {
          return stub->AsyncCancelOperation(cq, std::move(context), request);
        },
        &google::cloud::internal::ExtractLongRunningResultMetadata<
            gcsa::UpdateDatabaseDdlMetadata>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent, polling_policy_prototype_->clone(),
        __func__);
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
    struct EncryptionVisitor {
      explicit EncryptionVisitor(gcsa::RestoreDatabaseRequest& request)
          : request_(request) {}
      void operator()(DefaultEncryption const&) const {
        // No encryption_config => USE_CONFIG_DEFAULT_OR_BACKUP_ENCRYPTION.
        // That is, use the same encryption configuration as the backup.
      }
      void operator()(GoogleEncryption const&) const {
        auto* config = request_.mutable_encryption_config();
        config->set_encryption_type(
            gcsa::RestoreDatabaseEncryptionConfig::GOOGLE_DEFAULT_ENCRYPTION);
      }
      void operator()(CustomerManagedEncryption const& cme) const {
        auto* config = request_.mutable_encryption_config();
        config->set_encryption_type(
            gcsa::RestoreDatabaseEncryptionConfig::CUSTOMER_MANAGED_ENCRYPTION);
        config->set_kms_key_name(cme.encryption_key().FullName());
      }
      gcsa::RestoreDatabaseRequest& request_;
    };
    absl::visit(EncryptionVisitor(request), p.encryption_config);
    auto stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<gcsa::Database>(
        background_threads_->cq(), std::move(request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gcsa::RestoreDatabaseRequest const& request) {
          return stub->AsyncRestoreDatabase(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::GetOperationRequest const& request) {
          return stub->AsyncGetOperation(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::CancelOperationRequest const& request) {
          return stub->AsyncCancelOperation(cq, std::move(context), request);
        },
        &google::cloud::internal::ExtractLongRunningResultResponse<
            gcsa::Database>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent, polling_policy_prototype_->clone(),
        __func__);
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
    struct EncryptionVisitor {
      explicit EncryptionVisitor(gcsa::CreateBackupRequest& request)
          : request_(request) {}
      void operator()(DefaultEncryption const&) const {
        // No encryption_config => USE_DATABASE_ENCRYPTION.
        // That is, use the same encryption configuration as the database.
      }
      void operator()(GoogleEncryption const&) const {
        auto* config = request_.mutable_encryption_config();
        config->set_encryption_type(
            gcsa::CreateBackupEncryptionConfig::GOOGLE_DEFAULT_ENCRYPTION);
      }
      void operator()(CustomerManagedEncryption const& cme) const {
        auto* config = request_.mutable_encryption_config();
        config->set_encryption_type(
            gcsa::CreateBackupEncryptionConfig::CUSTOMER_MANAGED_ENCRYPTION);
        config->set_kms_key_name(cme.encryption_key().FullName());
      }
      gcsa::CreateBackupRequest& request_;
    };
    absl::visit(EncryptionVisitor(request), p.encryption_config);
    auto stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<gcsa::Backup>(
        background_threads_->cq(), std::move(request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gcsa::CreateBackupRequest const& request) {
          return stub->AsyncCreateBackup(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::GetOperationRequest const& request) {
          return stub->AsyncGetOperation(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::CancelOperationRequest const& request) {
          return stub->AsyncCancelOperation(cq, std::move(context), request);
        },
        &google::cloud::internal::ExtractLongRunningResultResponse<
            gcsa::Backup>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent, polling_policy_prototype_->clone(),
        __func__);
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
  std::shared_ptr<spanner_internal::DatabaseAdminStub> stub_;
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

std::shared_ptr<spanner::DatabaseAdminConnection> MakeDatabaseAdminConnection(
    Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 SpannerPolicyOptionList>(opts, __func__);
  opts = spanner_internal::DefaultAdminOptions(std::move(opts));
  auto stub = spanner_internal::CreateDefaultDatabaseAdminStub(opts);
  return std::make_shared<spanner::DatabaseAdminConnectionImpl>(
      std::move(stub), std::move(opts));
}

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options) {
  return MakeDatabaseAdminConnection(internal::MakeOptions(options));
}

std::shared_ptr<DatabaseAdminConnection> MakeDatabaseAdminConnection(
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy) {
  auto opts = internal::MakeOptions(options);
  opts.set<SpannerRetryPolicyOption>(std::move(retry_policy));
  opts.set<SpannerBackoffPolicyOption>(std::move(backoff_policy));
  opts.set<SpannerPollingPolicyOption>(std::move(polling_policy));
  return MakeDatabaseAdminConnection(std::move(opts));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

std::shared_ptr<spanner::DatabaseAdminConnection>
MakeDatabaseAdminConnectionForTesting(std::shared_ptr<DatabaseAdminStub> stub,
                                      Options opts) {
  opts = spanner_internal::DefaultAdminOptions(std::move(opts));
  return std::make_shared<spanner::DatabaseAdminConnectionImpl>(
      std::move(stub), std::move(opts));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
