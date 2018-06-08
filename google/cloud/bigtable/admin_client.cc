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

namespace {
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

  grpc::Status CreateTableFromSnapshot(
      grpc::ClientContext* context,
      btadmin::CreateTableFromSnapshotRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->CreateTableFromSnapshot(context, request, response);
  }

  grpc::Status ListTables(grpc::ClientContext* context,
                          btadmin::ListTablesRequest const& request,
                          btadmin::ListTablesResponse* response) override {
    return impl_.Stub()->ListTables(context, request, response);
  }

  grpc::Status GetTable(grpc::ClientContext* context,
                        btadmin::GetTableRequest const& request,
                        btadmin::Table* response) override {
    return impl_.Stub()->GetTable(context, request, response);
  }

  grpc::Status DeleteTable(grpc::ClientContext* context,
                           btadmin::DeleteTableRequest const& request,
                           google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteTable(context, request, response);
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

  grpc::Status SnapshotTable(
      grpc::ClientContext* context,
      btadmin::SnapshotTableRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->SnapshotTable(context, request, response);
  }

  grpc::Status GetSnapshot(grpc::ClientContext* context,
                           btadmin::GetSnapshotRequest const& request,
                           btadmin::Snapshot* response) override {
    return impl_.Stub()->GetSnapshot(context, request, response);
  }

  grpc::Status ListSnapshots(
      grpc::ClientContext* context,
      btadmin::ListSnapshotsRequest const& request,
      btadmin::ListSnapshotsResponse* response) override {
    return impl_.Stub()->ListSnapshots(context, request, response);
  }

  grpc::Status DeleteSnapshot(grpc::ClientContext* context,
                              btadmin::DeleteSnapshotRequest const& request,
                              google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteSnapshot(context, request, response);
  }

 private:
  std::string project_;
  Impl impl_;
};
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::shared_ptr<AdminClient> CreateDefaultAdminClient(std::string project,
                                                      ClientOptions options) {
  return std::make_shared<DefaultAdminClient>(std::move(project),
                                              std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
