// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_connection.h"
#include "google/cloud/bigtable/internal/admin_client_params.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include "google/cloud/bigtable/internal/logging_admin_client.h"
#include "google/cloud/log.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace btadmin = ::google::bigtable::admin::v2;

grpc::Status AdminClient::CreateBackup(grpc::ClientContext*,
                                       btadmin::CreateBackupRequest const&,
                                       google::longrunning::Operation*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::GetBackup(grpc::ClientContext*,
                                    btadmin::GetBackupRequest const&,
                                    btadmin::Backup*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::UpdateBackup(grpc::ClientContext*,
                                       btadmin::UpdateBackupRequest const&,
                                       btadmin::Backup*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::DeleteBackup(grpc::ClientContext*,
                                       btadmin::DeleteBackupRequest const&,
                                       google::protobuf::Empty*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::ListBackups(grpc::ClientContext*,
                                      btadmin::ListBackupsRequest const&,
                                      btadmin::ListBackupsResponse*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::RestoreTable(grpc::ClientContext*,
                                       btadmin::RestoreTableRequest const&,
                                       google::longrunning::Operation*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
AdminClient::AsyncCreateBackup(grpc::ClientContext*,
                               btadmin::CreateBackupRequest const&,
                               grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Backup>>
AdminClient::AsyncGetBackup(grpc::ClientContext*,
                            btadmin::GetBackupRequest const&,
                            grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Backup>>
AdminClient::AsyncUpdateBackup(grpc::ClientContext*,
                               btadmin::UpdateBackupRequest const&,
                               grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
AdminClient::AsyncDeleteBackup(grpc::ClientContext*,
                               btadmin::DeleteBackupRequest const&,
                               grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<btadmin::ListBackupsResponse>>
AdminClient::AsyncListBackups(grpc::ClientContext*,
                              btadmin::ListBackupsRequest const&,
                              grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
AdminClient::AsyncRestoreTable(grpc::ClientContext*,
                               btadmin::RestoreTableRequest const&,
                               grpc::CompletionQueue*) {
  return {};
}

namespace {

/**
 * An AdminClient for single-threaded programs that refreshes credentials on all
 * gRPC errors.
 *
 * This class should not be used by multiple threads, it makes no attempt to
 * protect its critical sections.  While it is rare that the admin interface
 * will be used by multiple threads, we should use the same approach here and in
 * the regular client to support multi-threaded programs.
 *
 * The class also aggressively reconnects on any gRPC errors. A future version
 * should only reconnect on those errors that indicate the credentials or
 * connections need refreshing.
 */
class DefaultAdminClient : public google::cloud::bigtable::AdminClient {
 public:
  DefaultAdminClient(std::string project, Options options)
      : project_(std::move(project)),
        user_project_(
            options.has<UserProjectOption>()
                ? absl::nullopt
                : absl::make_optional(options.get<UserProjectOption>())),
        impl_(options) {
    auto params = bigtable_internal::AdminClientParams(std::move(options));
    cq_ = params.options.get<GrpcCompletionQueueOption>();
    background_threads_ = std::move(params.background_threads);
    connection_ = bigtable_admin::MakeBigtableTableAdminConnection(
        std::move(params.options));
  }

  std::string const& project() const override { return project_; }
  std::shared_ptr<grpc::Channel> Channel() override { return impl_.Channel(); }
  void reset() override { return impl_.reset(); }

  grpc::Status CreateTable(grpc::ClientContext* context,
                           btadmin::CreateTableRequest const& request,
                           btadmin::Table* response) override {
    ApplyOptions(context);
    return impl_.Stub()->CreateTable(context, request, response);
  }

  grpc::Status ListTables(grpc::ClientContext* context,
                          btadmin::ListTablesRequest const& request,
                          btadmin::ListTablesResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->ListTables(context, request, response);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<btadmin::ListTablesResponse>>
  AsyncListTables(grpc::ClientContext* context,
                  btadmin::ListTablesRequest const& request,
                  grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncListTables(context, request, cq);
  }

  grpc::Status GetTable(grpc::ClientContext* context,
                        btadmin::GetTableRequest const& request,
                        btadmin::Table* response) override {
    ApplyOptions(context);
    return impl_.Stub()->GetTable(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
  AsyncGetTable(grpc::ClientContext* context,
                btadmin::GetTableRequest const& request,
                grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncGetTable(context, request, cq);
  }

  grpc::Status DeleteTable(grpc::ClientContext* context,
                           btadmin::DeleteTableRequest const& request,
                           google::protobuf::Empty* response) override {
    ApplyOptions(context);
    return impl_.Stub()->DeleteTable(context, request, response);
  }

  grpc::Status CreateBackup(grpc::ClientContext* context,
                            btadmin::CreateBackupRequest const& request,
                            google::longrunning::Operation* response) override {
    ApplyOptions(context);
    return impl_.Stub()->CreateBackup(context, request, response);
  }

  grpc::Status GetBackup(grpc::ClientContext* context,
                         btadmin::GetBackupRequest const& request,
                         btadmin::Backup* response) override {
    ApplyOptions(context);
    return impl_.Stub()->GetBackup(context, request, response);
  }

  grpc::Status UpdateBackup(grpc::ClientContext* context,
                            btadmin::UpdateBackupRequest const& request,
                            btadmin::Backup* response) override {
    ApplyOptions(context);
    return impl_.Stub()->UpdateBackup(context, request, response);
  }

  grpc::Status DeleteBackup(grpc::ClientContext* context,
                            btadmin::DeleteBackupRequest const& request,
                            google::protobuf::Empty* response) override {
    ApplyOptions(context);
    return impl_.Stub()->DeleteBackup(context, request, response);
  }

  grpc::Status ListBackups(grpc::ClientContext* context,
                           btadmin::ListBackupsRequest const& request,
                           btadmin::ListBackupsResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->ListBackups(context, request, response);
  }

  grpc::Status RestoreTable(grpc::ClientContext* context,
                            btadmin::RestoreTableRequest const& request,
                            google::longrunning::Operation* response) override {
    ApplyOptions(context);
    return impl_.Stub()->RestoreTable(context, request, response);
  }

  grpc::Status ModifyColumnFamilies(
      grpc::ClientContext* context,
      btadmin::ModifyColumnFamiliesRequest const& request,
      btadmin::Table* response) override {
    ApplyOptions(context);
    return impl_.Stub()->ModifyColumnFamilies(context, request, response);
  }

  grpc::Status DropRowRange(grpc::ClientContext* context,
                            btadmin::DropRowRangeRequest const& request,
                            google::protobuf::Empty* response) override {
    ApplyOptions(context);
    return impl_.Stub()->DropRowRange(context, request, response);
  }

  grpc::Status GenerateConsistencyToken(
      grpc::ClientContext* context,
      btadmin::GenerateConsistencyTokenRequest const& request,
      btadmin::GenerateConsistencyTokenResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->GenerateConsistencyToken(context, request, response);
  }

  grpc::Status CheckConsistency(
      grpc::ClientContext* context,
      btadmin::CheckConsistencyRequest const& request,
      btadmin::CheckConsistencyResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->CheckConsistency(context, request, response);
  }

  grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) override {
    ApplyOptions(context);
    auto stub = google::longrunning::Operations::NewStub(Channel());
    return stub->GetOperation(context, request, response);
  }

  grpc::Status GetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::GetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override {
    ApplyOptions(context);
    return impl_.Stub()->GetIamPolicy(context, request, response);
  }

  grpc::Status SetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::SetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override {
    ApplyOptions(context);
    return impl_.Stub()->SetIamPolicy(context, request, response);
  }

  grpc::Status TestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      google::iam::v1::TestIamPermissionsResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->TestIamPermissions(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
  AsyncCreateTable(grpc::ClientContext* context,
                   btadmin::CreateTableRequest const& request,
                   grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncCreateTable(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>>
  AsyncDeleteTable(grpc::ClientContext* context,
                   btadmin::DeleteTableRequest const& request,
                   grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncDeleteTable(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncCreateBackup(grpc::ClientContext* context,
                    btadmin::CreateBackupRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncCreateBackup(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Backup>>
  AsyncGetBackup(grpc::ClientContext* context,
                 btadmin::GetBackupRequest const& request,
                 grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncGetBackup(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Backup>>
  AsyncUpdateBackup(grpc::ClientContext* context,
                    btadmin::UpdateBackupRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncUpdateBackup(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteBackup(grpc::ClientContext* context,
                    btadmin::DeleteBackupRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncDeleteBackup(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<btadmin::ListBackupsResponse>>
  AsyncListBackups(grpc::ClientContext* context,
                   btadmin::ListBackupsRequest const& request,
                   grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncListBackups(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncRestoreTable(grpc::ClientContext* context,
                    btadmin::RestoreTableRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncRestoreTable(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
  AsyncModifyColumnFamilies(grpc::ClientContext* context,
                            btadmin::ModifyColumnFamiliesRequest const& request,
                            grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncModifyColumnFamilies(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDropRowRange(grpc::ClientContext* context,
                    btadmin::DropRowRangeRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncDropRowRange(context, request, cq);
  };

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      btadmin::GenerateConsistencyTokenResponse>>
  AsyncGenerateConsistencyToken(
      grpc::ClientContext* context,
      btadmin::GenerateConsistencyTokenRequest const& request,
      grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncGenerateConsistencyToken(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      btadmin::CheckConsistencyResponse>>
  AsyncCheckConsistency(grpc::ClientContext* context,
                        btadmin::CheckConsistencyRequest const& request,
                        grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncCheckConsistency(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncGetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::GetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncGetIamPolicy(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncSetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::SetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncSetIamPolicy(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::iam::v1::TestIamPermissionsResponse>>
  AsyncTestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncTestIamPermissions(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncGetOperation(grpc::ClientContext* context,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    auto stub = google::longrunning::Operations::NewStub(Channel());
    return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
        google::longrunning::Operation>>(
        stub->AsyncGetOperation(context, request, cq).release());
  }

 private:
  std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> connection()
      override {
    return connection_;
  }

  CompletionQueue cq() override { return cq_; }

  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return impl_.BackgroundThreadsFactory();
  }

  void ApplyOptions(grpc::ClientContext* context) {
    if (!user_project_) return;
    context->AddMetadata("x-goog-user-project", *user_project_);
  }

  std::string project_;
  absl::optional<std::string> user_project_;
  internal::CommonClient<btadmin::BigtableTableAdmin> impl_;
  CompletionQueue cq_;
  std::unique_ptr<BackgroundThreads> background_threads_;
  std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> connection_;
};

}  // namespace

std::shared_ptr<AdminClient> MakeAdminClient(std::string project,
                                             Options options) {
  options = internal::DefaultTableAdminOptions(std::move(options));
  bool tracing_enabled = google::cloud::internal::Contains(
      options.get<TracingComponentsOption>(), "rpc");
  auto tracing_options = options.get<GrpcTracingOptionsOption>();

  std::shared_ptr<AdminClient> client = std::make_shared<DefaultAdminClient>(
      std::move(project), std::move(options));
  if (tracing_enabled) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    client = std::make_shared<internal::LoggingAdminClient>(
        std::move(client), std::move(tracing_options));
  }
  return client;
}

std::shared_ptr<AdminClient> CreateDefaultAdminClient(std::string project,
                                                      ClientOptions options) {
  return MakeAdminClient(std::move(project),
                         internal::MakeOptions(std::move(options)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
