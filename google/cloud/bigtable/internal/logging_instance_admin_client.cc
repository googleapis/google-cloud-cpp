// Copyright 2020 Google Inc.
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

#include "google/cloud/bigtable/internal/logging_instance_admin_client.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/log.h"
#include "google/cloud/tracing_options.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btadmin = google::bigtable::admin::v2;
using ::google::cloud::internal::LogWrapper;

grpc::Status LoggingInstanceAdminClient::ListInstances(
    grpc::ClientContext* context, btadmin::ListInstancesRequest const& request,
    btadmin::ListInstancesResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::ListInstancesRequest const& request,
             btadmin::ListInstancesResponse* response) {
        return child_->ListInstances(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::CreateInstance(
    grpc::ClientContext* context, btadmin::CreateInstanceRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CreateInstanceRequest const& request,
             google::longrunning::Operation* response) {
        return child_->CreateInstance(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::UpdateInstance(
    grpc::ClientContext* context,
    btadmin::PartialUpdateInstanceRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::PartialUpdateInstanceRequest const& request,
             google::longrunning::Operation* response) {
        return child_->UpdateInstance(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::GetOperation(
    grpc::ClientContext* context,
    google::longrunning::GetOperationRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::longrunning::GetOperationRequest const& request,
             google::longrunning::Operation* response) {
        return child_->GetOperation(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::GetInstance(
    grpc::ClientContext* context, btadmin::GetInstanceRequest const& request,
    btadmin::Instance* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::GetInstanceRequest const& request,
             btadmin::Instance* response) {
        return child_->GetInstance(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::DeleteInstance(
    grpc::ClientContext* context, btadmin::DeleteInstanceRequest const& request,
    google::protobuf::Empty* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::DeleteInstanceRequest const& request,
             google::protobuf::Empty* response) {
        return child_->DeleteInstance(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::ListClusters(
    grpc::ClientContext* context, btadmin::ListClustersRequest const& request,
    btadmin::ListClustersResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::ListClustersRequest const& request,
             btadmin::ListClustersResponse* response) {
        return child_->ListClusters(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::GetCluster(
    grpc::ClientContext* context, btadmin::GetClusterRequest const& request,
    btadmin::Cluster* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::GetClusterRequest const& request,
             btadmin::Cluster* response) {
        return child_->GetCluster(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::DeleteCluster(
    grpc::ClientContext* context, btadmin::DeleteClusterRequest const& request,
    google::protobuf::Empty* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::DeleteClusterRequest const& request,
             google::protobuf::Empty* response) {
        return child_->DeleteCluster(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::CreateCluster(
    grpc::ClientContext* context, btadmin::CreateClusterRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CreateClusterRequest const& request,
             google::longrunning::Operation* response) {
        return child_->CreateCluster(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::UpdateCluster(
    grpc::ClientContext* context, btadmin::Cluster const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context, btadmin::Cluster const& request,
             google::longrunning::Operation* response) {
        return child_->UpdateCluster(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::CreateAppProfile(
    grpc::ClientContext* context,
    btadmin::CreateAppProfileRequest const& request,
    btadmin::AppProfile* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CreateAppProfileRequest const& request,
             btadmin::AppProfile* response) {
        return child_->CreateAppProfile(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::GetAppProfile(
    grpc::ClientContext* context, btadmin::GetAppProfileRequest const& request,
    btadmin::AppProfile* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::GetAppProfileRequest const& request,
             btadmin::AppProfile* response) {
        return child_->GetAppProfile(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::ListAppProfiles(
    grpc::ClientContext* context,
    btadmin::ListAppProfilesRequest const& request,
    btadmin::ListAppProfilesResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::ListAppProfilesRequest const& request,
             btadmin::ListAppProfilesResponse* response) {
        return child_->ListAppProfiles(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::UpdateAppProfile(
    grpc::ClientContext* context,
    btadmin::UpdateAppProfileRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::UpdateAppProfileRequest const& request,
             google::longrunning::Operation* response) {
        return child_->UpdateAppProfile(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::DeleteAppProfile(
    grpc::ClientContext* context,
    btadmin::DeleteAppProfileRequest const& request,
    google::protobuf::Empty* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::DeleteAppProfileRequest const& request,
             google::protobuf::Empty* response) {
        return child_->DeleteAppProfile(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::GetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::GetIamPolicyRequest const& request,
    google::iam::v1::Policy* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::iam::v1::GetIamPolicyRequest const& request,
             google::iam::v1::Policy* response) {
        return child_->GetIamPolicy(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::SetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::SetIamPolicyRequest const& request,
    google::iam::v1::Policy* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::iam::v1::SetIamPolicyRequest const& request,
             google::iam::v1::Policy* response) {
        return child_->SetIamPolicy(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingInstanceAdminClient::TestIamPermissions(
    grpc::ClientContext* context,
    google::iam::v1::TestIamPermissionsRequest const& request,
    google::iam::v1::TestIamPermissionsResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::iam::v1::TestIamPermissionsRequest const& request,
             google::iam::v1::TestIamPermissionsResponse* response) {
        return child_->TestIamPermissions(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<btadmin::ListInstancesResponse>>
LoggingInstanceAdminClient::AsyncListInstances(
    grpc::ClientContext* context, btadmin::ListInstancesRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncListInstances(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Instance>>
LoggingInstanceAdminClient::AsyncGetInstance(
    grpc::ClientContext* context, btadmin::GetInstanceRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetInstance(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Cluster>>
LoggingInstanceAdminClient::AsyncGetCluster(
    grpc::ClientContext* context, btadmin::GetClusterRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetCluster(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
LoggingInstanceAdminClient::AsyncDeleteCluster(
    grpc::ClientContext* context, btadmin::DeleteClusterRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncDeleteCluster(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingInstanceAdminClient::AsyncCreateCluster(
    grpc::ClientContext* context, const btadmin::CreateClusterRequest& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncCreateCluster(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingInstanceAdminClient::AsyncCreateInstance(
    grpc::ClientContext* context, const btadmin::CreateInstanceRequest& request,
    grpc::CompletionQueue* cq) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CreateInstanceRequest const& request,
             grpc::CompletionQueue* cq) {
        return child_->AsyncCreateInstance(context, request, cq);
      },
      context, request, cq, __func__, tracing_options_);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingInstanceAdminClient::AsyncUpdateInstance(
    grpc::ClientContext* context,
    const btadmin::PartialUpdateInstanceRequest& request,
    grpc::CompletionQueue* cq) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::PartialUpdateInstanceRequest const& request,
             grpc::CompletionQueue* cq) {
        return child_->AsyncUpdateInstance(context, request, cq);
      },
      context, request, cq, __func__, tracing_options_);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingInstanceAdminClient::AsyncUpdateCluster(grpc::ClientContext* context,
                                               const btadmin::Cluster& request,
                                               grpc::CompletionQueue* cq) {
  return child_->AsyncUpdateCluster(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
LoggingInstanceAdminClient::AsyncDeleteInstance(
    grpc::ClientContext* context, btadmin::DeleteInstanceRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncDeleteInstance(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<btadmin::ListClustersResponse>>
LoggingInstanceAdminClient::AsyncListClusters(
    grpc::ClientContext* context, const btadmin::ListClustersRequest& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncListClusters(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::AppProfile>>
LoggingInstanceAdminClient::AsyncGetAppProfile(
    grpc::ClientContext* context, btadmin::GetAppProfileRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetAppProfile(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
LoggingInstanceAdminClient::AsyncDeleteAppProfile(
    grpc::ClientContext* context,
    btadmin::DeleteAppProfileRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncDeleteAppProfile(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::AppProfile>>
LoggingInstanceAdminClient::AsyncCreateAppProfile(
    grpc::ClientContext* context,
    btadmin::CreateAppProfileRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncCreateAppProfile(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingInstanceAdminClient::AsyncUpdateAppProfile(
    grpc::ClientContext* context,
    const btadmin::UpdateAppProfileRequest& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncUpdateAppProfile(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<btadmin::ListAppProfilesResponse>>
LoggingInstanceAdminClient::AsyncListAppProfiles(
    grpc::ClientContext* context,
    const btadmin::ListAppProfilesRequest& request, grpc::CompletionQueue* cq) {
  return child_->AsyncListAppProfiles(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
LoggingInstanceAdminClient::AsyncGetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::GetIamPolicyRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetIamPolicy(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
LoggingInstanceAdminClient::AsyncSetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::SetIamPolicyRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncSetIamPolicy(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::iam::v1::TestIamPermissionsResponse>>
LoggingInstanceAdminClient::AsyncTestIamPermissions(
    grpc::ClientContext* context,
    google::iam::v1::TestIamPermissionsRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncTestIamPermissions(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingInstanceAdminClient::AsyncGetOperation(
    grpc::ClientContext* context,
    const google::longrunning::GetOperationRequest& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetOperation(context, request, cq);
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
