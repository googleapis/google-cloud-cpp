// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include "google/cloud/bigtable/internal/logging_admin_client.h"
#include "google/cloud/log.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
grpc::Status AdminClient::CreateBackup(
    grpc::ClientContext*,
    google::bigtable::admin::v2::CreateBackupRequest const&,
    google::longrunning::Operation*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::GetBackup(
    grpc::ClientContext*, google::bigtable::admin::v2::GetBackupRequest const&,
    google::bigtable::admin::v2::Backup*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::UpdateBackup(
    grpc::ClientContext*,
    google::bigtable::admin::v2::UpdateBackupRequest const&,
    google::bigtable::admin::v2::Backup*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::DeleteBackup(
    grpc::ClientContext*,
    google::bigtable::admin::v2::DeleteBackupRequest const&,
    google::protobuf::Empty*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::ListBackups(
    grpc::ClientContext*,
    google::bigtable::admin::v2::ListBackupsRequest const&,
    google::bigtable::admin::v2::ListBackupsResponse*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

grpc::Status AdminClient::RestoreTable(
    grpc::ClientContext*,
    google::bigtable::admin::v2::RestoreTableRequest const&,
    google::longrunning::Operation*) {
  return {grpc::StatusCode::UNIMPLEMENTED, "Not implemented"};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
AdminClient::AsyncCreateBackup(
    grpc::ClientContext*,
    google::bigtable::admin::v2::CreateBackupRequest const&,
    grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Backup>>
AdminClient::AsyncGetBackup(
    grpc::ClientContext*, google::bigtable::admin::v2::GetBackupRequest const&,
    grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Backup>>
AdminClient::AsyncUpdateBackup(
    grpc::ClientContext*,
    google::bigtable::admin::v2::UpdateBackupRequest const&,
    grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
AdminClient::AsyncDeleteBackup(
    grpc::ClientContext*,
    google::bigtable::admin::v2::DeleteBackupRequest const&,
    grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::ListBackupsResponse>>
AdminClient::AsyncListBackups(
    grpc::ClientContext*,
    google::bigtable::admin::v2::ListBackupsRequest const&,
    grpc::CompletionQueue*) {
  return {};
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
AdminClient::AsyncRestoreTable(
    grpc::ClientContext*,
    google::bigtable::admin::v2::RestoreTableRequest const&,
    grpc::CompletionQueue*) {
  return {};
}

namespace btadmin = google::bigtable::admin::v2;

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
 private:
  // Introduce an early `private:` section because this type is used to define
  // the public interface, it should not be part of the public interface.
  struct AdminTraits {
    static std::string const& Endpoint(
        google::cloud::bigtable::ClientOptions& options) {
      return options.admin_endpoint();
    }
  };

  using Impl = google::cloud::bigtable::internal::CommonClient<
      AdminTraits, btadmin::BigtableTableAdmin>;

 public:
  using AdminStubPtr = Impl::StubPtr;

  DefaultAdminClient(std::string project,
                     google::cloud::bigtable::ClientOptions options)
      : project_(std::move(project)), impl_(std::move(options)) {}

  std::string const& project() const override { return project_; }
  std::shared_ptr<grpc::Channel> Channel() override { return impl_.Channel(); }
  void reset() override { return impl_.reset(); }

  grpc::Status CreateTable(grpc::ClientContext* context,
                           btadmin::CreateTableRequest const& request,
                           btadmin::Table* response) override {
    return impl_.Stub()->CreateTable(context, request, response);
  }

  grpc::Status ListTables(grpc::ClientContext* context,
                          btadmin::ListTablesRequest const& request,
                          btadmin::ListTablesResponse* response) override {
    return impl_.Stub()->ListTables(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListTablesResponse>>
  AsyncListTables(grpc::ClientContext* context,
                  google::bigtable::admin::v2::ListTablesRequest const& request,
                  grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncListTables(context, request, cq);
  }

  grpc::Status GetTable(grpc::ClientContext* context,
                        btadmin::GetTableRequest const& request,
                        btadmin::Table* response) override {
    return impl_.Stub()->GetTable(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncGetTable(grpc::ClientContext* context,
                google::bigtable::admin::v2::GetTableRequest const& request,
                grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncGetTable(context, request, cq);
  }

  grpc::Status DeleteTable(grpc::ClientContext* context,
                           btadmin::DeleteTableRequest const& request,
                           google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteTable(context, request, response);
  }

  grpc::Status CreateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateBackupRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->CreateBackup(context, request, response);
  }

  grpc::Status GetBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetBackupRequest const& request,
      google::bigtable::admin::v2::Backup* response) override {
    return impl_.Stub()->GetBackup(context, request, response);
  }

  grpc::Status UpdateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateBackupRequest const& request,
      google::bigtable::admin::v2::Backup* response) override {
    return impl_.Stub()->UpdateBackup(context, request, response);
  }

  grpc::Status DeleteBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteBackupRequest const& request,
      google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteBackup(context, request, response);
  }

  grpc::Status ListBackups(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListBackupsRequest const& request,
      google::bigtable::admin::v2::ListBackupsResponse* response) override {
    return impl_.Stub()->ListBackups(context, request, response);
  }

  grpc::Status RestoreTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::RestoreTableRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->RestoreTable(context, request, response);
  }

  grpc::Status ModifyColumnFamilies(
      grpc::ClientContext* context,
      btadmin::ModifyColumnFamiliesRequest const& request,
      btadmin::Table* response) override {
    return impl_.Stub()->ModifyColumnFamilies(context, request, response);
  }

  grpc::Status DropRowRange(grpc::ClientContext* context,
                            btadmin::DropRowRangeRequest const& request,
                            google::protobuf::Empty* response) override {
    return impl_.Stub()->DropRowRange(context, request, response);
  }

  grpc::Status GenerateConsistencyToken(
      grpc::ClientContext* context,
      btadmin::GenerateConsistencyTokenRequest const& request,
      btadmin::GenerateConsistencyTokenResponse* response) override {
    return impl_.Stub()->GenerateConsistencyToken(context, request, response);
  }

  grpc::Status CheckConsistency(
      grpc::ClientContext* context,
      btadmin::CheckConsistencyRequest const& request,
      btadmin::CheckConsistencyResponse* response) override {
    return impl_.Stub()->CheckConsistency(context, request, response);
  }

  grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) override {
    auto stub = google::longrunning::Operations::NewStub(Channel());
    return stub->GetOperation(context, request, response);
  }

  grpc::Status GetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::GetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override {
    return impl_.Stub()->GetIamPolicy(context, request, response);
  }

  grpc::Status SetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::SetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override {
    return impl_.Stub()->SetIamPolicy(context, request, response);
  }

  grpc::Status TestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      google::iam::v1::TestIamPermissionsResponse* response) override {
    return impl_.Stub()->TestIamPermissions(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncCreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncCreateTable(context, request, cq);
  }

  std::unique_ptr<
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>>
  AsyncDeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncDeleteTable(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncCreateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateBackupRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncCreateBackup(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Backup>>
  AsyncGetBackup(grpc::ClientContext* context,
                 google::bigtable::admin::v2::GetBackupRequest const& request,
                 grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncGetBackup(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Backup>>
  AsyncUpdateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateBackupRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncUpdateBackup(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteBackupRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncDeleteBackup(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListBackupsResponse>>
  AsyncListBackups(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListBackupsRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncListBackups(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncRestoreTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::RestoreTableRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncRestoreTable(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncModifyColumnFamilies(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncModifyColumnFamilies(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDropRowRange(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DropRowRangeRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncDropRowRange(context, request, cq);
  };

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>
  AsyncGenerateConsistencyToken(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
          request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncGenerateConsistencyToken(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::CheckConsistencyResponse>>
  AsyncCheckConsistency(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CheckConsistencyRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncCheckConsistency(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncGetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::GetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncGetIamPolicy(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncSetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::SetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncSetIamPolicy(context, request, cq);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::iam::v1::TestIamPermissionsResponse>>
  AsyncTestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncTestIamPermissions(context, request, cq);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncGetOperation(grpc::ClientContext* context,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue* cq) override {
    auto stub = google::longrunning::Operations::NewStub(Channel());
    return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
        google::longrunning::Operation>>(
        stub->AsyncGetOperation(context, request, cq).release());
  }

 private:
  ClientOptions::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return impl_.Options().background_threads_factory();
  }

  std::string project_;
  Impl impl_;
};

std::shared_ptr<AdminClient> CreateDefaultAdminClient(
    std::string project,
    ClientOptions options) {  // NOLINT(performance-unnecessary-value-param)
  std::shared_ptr<AdminClient> client =
      std::make_shared<DefaultAdminClient>(std::move(project), options);
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    client = std::make_shared<internal::LoggingAdminClient>(
        std::move(client), options.tracing_options());
  }
  return client;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
