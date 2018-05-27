// Copyright 2018 Google LLC
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

#include "bigtable/client/internal/make_unique.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <grpc++/grpc++.h>

namespace btadmin = google::bigtable::admin::v2;

/**
 * In-memory implementation of `google.bigtable.admin.v2.InstanceAdmin`.
 *
 * This implementation is intended to test the client library APIs to manipulate
 * instances, clusters, app profiles, and IAM permissions. Applications should
 * not use it for testing or development, please consider using mocks instead.
 */
class InstanceAdminEmulator final
    : public btadmin::BigtableInstanceAdmin::Service {
 public:
  InstanceAdminEmulator() = default;

  grpc::Status CreateInstance(
      grpc::ServerContext* context,
      btadmin::CreateInstanceRequest const* request,
      google::longrunning::Operation* response) override {
    std::string name =
        request->parent() + "/instances/" + request->instance_id();
    auto ins = instances_.emplace(name, request->instance());
    if (ins.second) {
      auto& stored_instance = ins.first->second;
      stored_instance.set_name(name);
      stored_instance.set_state(btadmin::Instance::READY);
      response->set_name("create-instance/" + name);
      response->set_done(true);
      auto contents = bigtable::internal::make_unique<google::protobuf::Any>();
      contents->PackFrom(stored_instance);
      response->set_allocated_response(contents.release());
      return grpc::Status::OK;
    }
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "duplicate instance");
  }

  grpc::Status GetInstance(grpc::ServerContext* context,
                           btadmin::GetInstanceRequest const* request,
                           btadmin::Instance* response) override {
    auto i = instances_.find(request->name());
    if (i == instances_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "instance missing");
    }
    *response = i->second;
    return grpc::Status::OK;
  }

  grpc::Status ListInstances(
      grpc::ServerContext* context,
      btadmin::ListInstancesRequest const* request,
      btadmin::ListInstancesResponse* response) override {
    std::string prefix = request->parent() + "/instances/";
    for (auto const& kv : instances_) {
      if (0 == kv.first.find(prefix)) {
        *response->add_instances() = kv.second;
      }
    }
    return grpc::Status::OK;
  }

  grpc::Status UpdateInstance(grpc::ServerContext* context,
                              btadmin::Instance const* request,
                              btadmin::Instance* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status PartialUpdateInstance(
      grpc::ServerContext* context,
      btadmin::PartialUpdateInstanceRequest const* request,
      google::longrunning::Operation* response) override {
    std::string name = request->instance().name();
    auto it = instances_.find(name);
    if (it == instances_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "instance not found");
    }

    auto& stored_instance = it->second;

    for (int index = 0; index != request->update_mask().paths_size(); ++index) {
      if ("display_name" == request->update_mask().paths(index)) {
        stored_instance.set_display_name(request->instance().display_name());
      }
      if ("name" == request->update_mask().paths(index)) {
        stored_instance.set_display_name(request->instance().name());
      }
      if ("state" == request->update_mask().paths(index)) {
        stored_instance.set_state(request->instance().state());
      }
      if ("type" == request->update_mask().paths(index)) {
        stored_instance.set_type(request->instance().type());
      }
      if ("labels" == request->update_mask().paths(index)) {
        stored_instance.set_type(request->instance().type());
        stored_instance.mutable_labels()->clear();
        stored_instance.mutable_labels()->insert(
            request->instance().labels().begin(),
            request->instance().labels().end());
      }
    }
    response->set_name("update-instance/" + name);
    response->set_done(true);
    auto contents = bigtable::internal::make_unique<google::protobuf::Any>();
    contents->PackFrom(stored_instance);
    response->set_allocated_response(contents.release());
    return grpc::Status::OK;
  }

  grpc::Status DeleteInstance(grpc::ServerContext* context,
                              btadmin::DeleteInstanceRequest const* request,
                              google::protobuf::Empty* response) override {
    auto i = instances_.find(request->name());
    if (i == instances_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "instance missing");
    }
    instances_.erase(i);
    return grpc::Status::OK;
  }

  grpc::Status CreateCluster(
      grpc::ServerContext* context,
      btadmin::CreateClusterRequest const* request,
      google::longrunning::Operation* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status GetCluster(grpc::ServerContext* context,
                          btadmin::GetClusterRequest const* request,
                          btadmin::Cluster* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status ListClusters(grpc::ServerContext* context,
                            btadmin::ListClustersRequest const* request,
                            btadmin::ListClustersResponse* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status UpdateCluster(
      grpc::ServerContext* context, btadmin::Cluster const* request,
      google::longrunning::Operation* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status DeleteCluster(grpc::ServerContext* context,
                             btadmin::DeleteClusterRequest const* request,
                             google::protobuf::Empty* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status CreateAppProfile(grpc::ServerContext* context,
                                btadmin::CreateAppProfileRequest const* request,
                                btadmin::AppProfile* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status GetAppProfile(grpc::ServerContext* context,
                             btadmin::GetAppProfileRequest const* request,
                             btadmin::AppProfile* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status ListAppProfiles(
      grpc::ServerContext* context,
      btadmin::ListAppProfilesRequest const* request,
      btadmin::ListAppProfilesResponse* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status UpdateAppProfile(
      grpc::ServerContext* context,
      btadmin::UpdateAppProfileRequest const* request,
      google::longrunning::Operation* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status DeleteAppProfile(grpc::ServerContext* context,
                                btadmin::DeleteAppProfileRequest const* request,
                                google::protobuf::Empty* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status GetIamPolicy(grpc::ServerContext* context,
                            google::iam::v1::GetIamPolicyRequest const* request,
                            google::iam::v1::Policy* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status SetIamPolicy(grpc::ServerContext* context,
                            google::iam::v1::SetIamPolicyRequest const* request,
                            google::iam::v1::Policy* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status TestIamPermissions(
      grpc::ServerContext* context,
      google::iam::v1::TestIamPermissionsRequest const* request,
      google::iam::v1::TestIamPermissionsResponse* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

 private:
  std::map<std::string, btadmin::Instance> instances_;
  std::map<std::string, google::longrunning::Operation> pending_operations_;
};

class LongRunningEmulator final
    : public google::longrunning::Operations::Service {
 public:
  explicit LongRunningEmulator() {}

  virtual grpc::Status ListOperations(
      grpc::ServerContext* context,
      google::longrunning::ListOperationsRequest const* request,
      google::longrunning::ListOperationsResponse* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status GetOperation(
      grpc::ServerContext* context,
      google::longrunning::GetOperationRequest const* request,
      google::longrunning::Operation* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status DeleteOperation(
      grpc::ServerContext* context,
      google::longrunning::DeleteOperationRequest const* request,
      google::protobuf::Empty* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status CancelOperation(
      grpc::ServerContext* context,
      google::longrunning::CancelOperationRequest const* request,
      google::protobuf::Empty* response) override {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }
};

/// The implementation of EmbeddedServer.
class DefaultEmbeddedServer {
 public:
  explicit DefaultEmbeddedServer(std::string server_address) {
    int port;
    builder_.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                              &port);
    builder_.RegisterService(&instance_admin_);
    builder_.RegisterService(&long_running_);
    server_ = builder_.BuildAndStart();
    address_ = "localhost:" + std::to_string(port);
  }

  std::string address() const { return address_; }
  void Shutdown() { server_->Shutdown(); }
  void Wait() { server_->Wait(); }

 private:
  InstanceAdminEmulator instance_admin_;
  LongRunningEmulator long_running_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
  std::string address_;
};

int run_server(int argc, char* argv[]) {
  std::string server_address("[::]:");
  if (argc == 2) {
    server_address += argv[1];
  } else {
    server_address += "9090";
  }
  DefaultEmbeddedServer server(server_address);
  std::cout << "Listening on " << server.address();
  server.Wait();

  return 0;
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
int main(int argc, char* argv[]) try {
  return run_server(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ Exception raised: " << ex.what() << std::endl;
  return 1;
}
#else
int main(int argc, char* argv[]) { return run_server(argc, argv); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
