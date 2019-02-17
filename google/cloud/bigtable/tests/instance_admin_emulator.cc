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

#include "google/cloud/bigtable/internal/prefix_range_end.h"
#include "google/cloud/internal/make_unique.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <google/protobuf/text_format.h>
#include <grpcpp/grpcpp.h>

namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

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
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    std::string name =
        request->parent() + "/instances/" + request->instance_id();
    auto ins = instances_.emplace(name, request->instance());

    if (ins.second) {
      auto& stored_instance = ins.first->second;
      stored_instance.set_name(name);
      stored_instance.set_state(btadmin::Instance::READY);
      response->set_name("create-instance/" + name);
      response->set_done(true);
      auto contents =
          google::cloud::internal::make_unique<google::protobuf::Any>();
      contents->PackFrom(stored_instance);
      response->set_allocated_response(contents.release());

      // Add cluster into clusters_
      for (auto& kv : request->clusters()) {
        auto cluster = kv.second;
        auto cluster_name = name + "/clusters/" + kv.first;
        cluster.set_name(cluster_name);
        clusters_.emplace(std::move(cluster_name), std::move(cluster));
      }

      return grpc::Status::OK;
    }
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "duplicate instance");
  }

  grpc::Status GetInstance(grpc::ServerContext* context,
                           btadmin::GetInstanceRequest const* request,
                           btadmin::Instance* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

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
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

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
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "not implemented");
  }

  grpc::Status PartialUpdateInstance(
      grpc::ServerContext* context,
      btadmin::PartialUpdateInstanceRequest const* request,
      google::longrunning::Operation* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    std::string name = request->instance().name();
    auto it = instances_.find(name);
    if (it == instances_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "instance not found");
    }

    auto& stored_instance = it->second;

    for (int index = 0; index != request->update_mask().paths_size(); ++index) {
      if ("display_name" == request->update_mask().paths(index)) {
        auto size = request->instance().display_name().size();
        if (size < 4 || size > 30) {
          return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                              "display name size must be in range [4,30]");
        }
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
    auto contents =
        google::cloud::internal::make_unique<google::protobuf::Any>();
    contents->PackFrom(stored_instance);
    response->set_allocated_response(contents.release());
    return grpc::Status::OK;
  }

  grpc::Status DeleteInstance(grpc::ServerContext* context,
                              btadmin::DeleteInstanceRequest const* request,
                              google::protobuf::Empty* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto i = instances_.find(request->name());
    if (i == instances_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "instance missing");
    }
    instances_.erase(i->first);

    std::string prefix_successor =
        bigtable::internal::PrefixRangeEnd(request->name());
    auto begin = clusters_.lower_bound(request->name());
    auto end = clusters_.upper_bound(prefix_successor);
    clusters_.erase(begin, end);

    return grpc::Status::OK;
  }

  grpc::Status CreateCluster(
      grpc::ServerContext* context,
      btadmin::CreateClusterRequest const* request,
      google::longrunning::Operation* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    std::string name = request->parent() + "/clusters/" + request->cluster_id();
    auto ins = clusters_.emplace(name, request->cluster());
    if (ins.second) {
      auto& stored_cluster = ins.first->second;
      stored_cluster.set_name(name);
      stored_cluster.set_state(btadmin::Cluster::READY);
      response->set_name("create-cluster/" + name);
      response->set_done(true);
      auto contents =
          google::cloud::internal::make_unique<google::protobuf::Any>();
      contents->PackFrom(stored_cluster);
      response->set_allocated_response(contents.release());
      return grpc::Status::OK;
    }
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "duplicate cluster");
  }

  grpc::Status GetCluster(grpc::ServerContext* context,
                          btadmin::GetClusterRequest const* request,
                          btadmin::Cluster* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto i = clusters_.find(request->name());
    if (i == clusters_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "cluster missing");
    }
    *response = i->second;
    return grpc::Status::OK;
  }

  grpc::Status ListClusters(grpc::ServerContext* context,
                            btadmin::ListClustersRequest const* request,
                            btadmin::ListClustersResponse* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    // We should only return the clusters for the project embedded in the
    // request->parent() field. Do some simple (and naive) parsing, ignore
    // malformed requests for now.
    std::string project_path =
        request->parent().substr(0, request->parent().find("/instances"));

    auto return_all_clusters = false;
    // This is only used if we are returning the clusters for an specific
    // instance.
    std::string prefix;
    // If the instance name is "-" then we are supposed to return all clusters,
    // otherwise we just return the clusters for the given instance. Parse some
    // more to figure out if all clusters should be returned.
    if (request->parent() == project_path + "/instances/-") {
      return_all_clusters = true;
    } else {
      prefix = request->parent() + "/clusters/";
    }

    for (auto const& cluster : clusters_) {
      // Ignore clusters for other projects.
      if (std::string::npos == cluster.first.find(project_path)) {
        continue;
      }
      if (return_all_clusters) {
        *response->add_clusters() = cluster.second;
      } else if (std::string::npos != cluster.first.find(prefix)) {
        *response->add_clusters() = cluster.second;
      }
    }

    return grpc::Status::OK;
  }

  grpc::Status UpdateCluster(
      grpc::ServerContext* context, btadmin::Cluster const* request,
      google::longrunning::Operation* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    std::string name = request->name();
    auto it = clusters_.find(name);
    if (it == clusters_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "cluster not found");
    }
    auto& stored_cluster = it->second;
    stored_cluster.CopyFrom(*request);
    response->set_name("update-cluster/" + name);
    response->set_done(true);
    auto contents =
        google::cloud::internal::make_unique<google::protobuf::Any>();
    contents->PackFrom(stored_cluster);
    response->set_allocated_response(contents.release());
    return grpc::Status::OK;
  }

  grpc::Status DeleteCluster(grpc::ServerContext* context,
                             btadmin::DeleteClusterRequest const* request,
                             google::protobuf::Empty* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto i = clusters_.find(request->name());
    if (i == clusters_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "instance missing");
    }
    clusters_.erase(i->first);
    return grpc::Status::OK;
  }

  grpc::Status CreateAppProfile(grpc::ServerContext* context,
                                btadmin::CreateAppProfileRequest const* request,
                                btadmin::AppProfile* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto name = request->parent() + "/appProfiles/" + request->app_profile_id();
    auto ins = app_profiles_.emplace(name, request->app_profile());
    if (ins.second) {
      auto& profile = ins.first->second;
      profile.set_name(std::move(name));
      profile.set_etag("xyz" + std::to_string(++counter_));
      *response = profile;
      return grpc::Status::OK;
    }
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS,
                        "duplicate app profile");
  }

  grpc::Status GetAppProfile(grpc::ServerContext* context,
                             btadmin::GetAppProfileRequest const* request,
                             btadmin::AppProfile* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto i = app_profiles_.find(request->name());
    if (i == app_profiles_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "app profile not found");
    }
    *response = i->second;
    return grpc::Status::OK;
  }

  grpc::Status ListAppProfiles(
      grpc::ServerContext* context,
      btadmin::ListAppProfilesRequest const* request,
      btadmin::ListAppProfilesResponse* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto const& parent = request->parent();
    for (auto const& kv : app_profiles_) {
      if (0 == kv.first.find(parent)) {
        *response->add_app_profiles() = kv.second;
      }
    }
    response->mutable_next_page_token()->clear();
    return grpc::Status::OK;
  }

  grpc::Status UpdateAppProfile(
      grpc::ServerContext* context,
      btadmin::UpdateAppProfileRequest const* request,
      google::longrunning::Operation* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    std::string name = request->app_profile().name();
    auto it = app_profiles_.find(name);
    if (it == app_profiles_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "app profile not found");
    }

    btadmin::AppProfile& stored_app_profile = it->second;

    for (int index = 0; index != request->update_mask().paths_size(); ++index) {
      if ("description" == request->update_mask().paths(index)) {
        stored_app_profile.set_description(
            request->app_profile().description());
      }
      if ("multi_cluster_routing_policy_use_any" ==
          request->update_mask().paths(index)) {
        *stored_app_profile.mutable_multi_cluster_routing_use_any() =
            request->app_profile().multi_cluster_routing_use_any();
      }
      if ("single_cluster_routing" == request->update_mask().paths(index)) {
        *stored_app_profile.mutable_single_cluster_routing() =
            request->app_profile().single_cluster_routing();
      }
    }
    response->set_name("update-app-profile/" + name);
    response->set_done(true);
    auto contents =
        google::cloud::internal::make_unique<google::protobuf::Any>();
    contents->PackFrom(stored_app_profile);
    response->set_allocated_response(contents.release());
    return grpc::Status::OK;
  }

  grpc::Status DeleteAppProfile(grpc::ServerContext* context,
                                btadmin::DeleteAppProfileRequest const* request,
                                google::protobuf::Empty* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto i = app_profiles_.find(request->name());
    if (i == app_profiles_.end()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "app profile not found");
    }
    app_profiles_.erase(i);
    return grpc::Status::OK;
  }

  grpc::Status GetIamPolicy(grpc::ServerContext* context,
                            google::iam::v1::GetIamPolicyRequest const* request,
                            google::iam::v1::Policy* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto it = policies_.find(request->resource());
    if (it == policies_.end()) {
      *response = google::iam::v1::Policy();
    } else {
      *response = it->second;
    }

    return grpc::Status::OK;
  }

  grpc::Status SetIamPolicy(grpc::ServerContext* context,
                            google::iam::v1::SetIamPolicyRequest const* request,
                            google::iam::v1::Policy* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto policy = request->policy();
    *response = policy;
    policies_[request->resource()] = policy;

    return grpc::Status::OK;
  }

  grpc::Status TestIamPermissions(
      grpc::ServerContext* context,
      google::iam::v1::TestIamPermissionsRequest const* request,
      google::iam::v1::TestIamPermissionsResponse* response) override {
    std::unique_lock<std::mutex> lk(mu_);
    std::string request_text;
    google::protobuf::TextFormat::PrintToString(*request, &request_text);
    std::cout << __func__ << "() request=" << request_text << "\n";

    auto it = instances_.find(request->resource());
    if (it != instances_.end()) {
      for (auto const& p : request->permissions()) {
        response->add_permissions(p);
      }
      return grpc::Status::OK;
    }

    return grpc::Status(grpc::StatusCode::NOT_FOUND, "resource doesn't exists");
  }

 private:
  std::mutex mu_;
  std::map<std::string, btadmin::Instance> instances_;
  std::map<std::string, btadmin::Cluster> clusters_;
  std::map<std::string, google::longrunning::Operation> pending_operations_;
  std::map<std::string, btadmin::AppProfile> app_profiles_;
  std::map<std::string, google::iam::v1::Policy> policies_;
  long counter_ = 0;
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
    server_address += "0";
  }
  DefaultEmbeddedServer server(server_address);
  // Need to flush so the output becomes immediately visible to the driver
  // scripts.
  std::cout << "Cloud Bigtable emulator running on " << server.address() << '\n'
            << std::flush;
  server.Wait();

  return 0;
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
int main(int argc, char* argv[]) try {
  return run_server(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ Exception raised: " << ex.what() << "\n";
  return 1;
}
#else
int main(int argc, char* argv[]) { return run_server(argc, argv); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
