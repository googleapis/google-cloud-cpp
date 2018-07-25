// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace bigtable = google::cloud::bigtable;

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
class DefaultInstanceAdminClient : public bigtable::InstanceAdminClient {
 private:
  // Introduce an early `private:` section because this type is used to define
  // the public interface, it should not be part of the public interface.
  struct AdminTraits {
    static std::string const& Endpoint(bigtable::ClientOptions& options) {
      return options.admin_endpoint();
    }
  };

  using Impl = bigtable::internal::CommonClient<
      AdminTraits, ::google::bigtable::admin::v2::BigtableInstanceAdmin>;

 public:
  using AdminStubPtr = Impl::StubPtr;

  DefaultInstanceAdminClient(std::string project,
                             bigtable::ClientOptions options)
      : project_(std::move(project)), impl_(std::move(options)) {}

  std::string const& project() const override { return project_; }
  Impl::ChannelPtr Channel() override { return impl_.Channel(); }
  void reset() override { return impl_.reset(); }

  grpc::Status ListInstances(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListInstancesRequest const& request,
      google::bigtable::admin::v2::ListInstancesResponse* response) override {
    return impl_.Stub()->ListInstances(context, request, response);
  }

  grpc::Status CreateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateInstanceRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->CreateInstance(context, request, response);
  }

  grpc::Status UpdateInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::PartialUpdateInstanceRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->PartialUpdateInstance(context, request, response);
  }

  grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) override {
    auto stub = google::longrunning::Operations::NewStub(Channel());
    return stub->GetOperation(context, request, response);
  }

  grpc::Status GetInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetInstanceRequest const& request,
      google::bigtable::admin::v2::Instance* response) override {
    return impl_.Stub()->GetInstance(context, request, response);
  }

  grpc::Status DeleteInstance(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteInstanceRequest const& request,
      google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteInstance(context, request, response);
  }

  grpc::Status ListClusters(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListClustersRequest const& request,
      google::bigtable::admin::v2::ListClustersResponse* response) override {
    return impl_.Stub()->ListClusters(context, request, response);
  }

  grpc::Status GetCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetClusterRequest const& request,
      google::bigtable::admin::v2::Cluster* response) override {
    return impl_.Stub()->GetCluster(context, request, response);
  }

  grpc::Status DeleteCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteClusterRequest const& request,
      google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteCluster(context, request, response);
  }

  grpc::Status CreateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateClusterRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->CreateCluster(context, request, response);
  }

  grpc::Status UpdateCluster(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::Cluster const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->UpdateCluster(context, request, response);
  }

  grpc::Status CreateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateAppProfileRequest const& request,
      google::bigtable::admin::v2::AppProfile* response) override {
    return impl_.Stub()->CreateAppProfile(context, request, response);
  }

  grpc::Status GetAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetAppProfileRequest const& request,
      google::bigtable::admin::v2::AppProfile* response) override {
    return impl_.Stub()->GetAppProfile(context, request, response);
  }

  grpc::Status ListAppProfiles(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListAppProfilesRequest const& request,
      google::bigtable::admin::v2::ListAppProfilesResponse* response) override {
    return impl_.Stub()->ListAppProfiles(context, request, response);
  }

  grpc::Status UpdateAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateAppProfileRequest const& request,
      google::longrunning::Operation* response) override {
    return impl_.Stub()->UpdateAppProfile(context, request, response);
  }

  grpc::Status DeleteAppProfile(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteAppProfileRequest const& request,
      google::protobuf::Empty* response) override {
    return impl_.Stub()->DeleteAppProfile(context, request, response);
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

  DefaultInstanceAdminClient(DefaultInstanceAdminClient const&) = delete;
  DefaultInstanceAdminClient& operator=(DefaultInstanceAdminClient const&) =
      delete;

 private:
  std::string project_;
  Impl impl_;
};
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::shared_ptr<InstanceAdminClient> CreateDefaultInstanceAdminClient(
    std::string project, ClientOptions options) {
  return std::make_shared<DefaultInstanceAdminClient>(std::move(project),
                                                      std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
