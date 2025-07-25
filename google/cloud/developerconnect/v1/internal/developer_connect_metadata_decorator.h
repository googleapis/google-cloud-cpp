// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/developerconnect/v1/developer_connect.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DEVELOPERCONNECT_V1_INTERNAL_DEVELOPER_CONNECT_METADATA_DECORATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DEVELOPERCONNECT_V1_INTERNAL_DEVELOPER_CONNECT_METADATA_DECORATOR_H

#include "google/cloud/developerconnect/v1/internal/developer_connect_stub.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <map>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace developerconnect_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class DeveloperConnectMetadata : public DeveloperConnectStub {
 public:
  ~DeveloperConnectMetadata() override = default;
  DeveloperConnectMetadata(
      std::shared_ptr<DeveloperConnectStub> child,
      std::multimap<std::string, std::string> fixed_metadata,
      std::string api_client_header = "");

  StatusOr<google::cloud::developerconnect::v1::ListConnectionsResponse>
  ListConnections(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::ListConnectionsRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::Connection> GetConnection(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::GetConnectionRequest const& request)
      override;

  future<StatusOr<google::longrunning::Operation>> AsyncCreateConnection(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::CreateConnectionRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> CreateConnection(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::CreateConnectionRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncUpdateConnection(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::UpdateConnectionRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> UpdateConnection(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::UpdateConnectionRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncDeleteConnection(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::DeleteConnectionRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> DeleteConnection(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::DeleteConnectionRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncCreateGitRepositoryLink(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::CreateGitRepositoryLinkRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> CreateGitRepositoryLink(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::CreateGitRepositoryLinkRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncDeleteGitRepositoryLink(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::DeleteGitRepositoryLinkRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> DeleteGitRepositoryLink(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::DeleteGitRepositoryLinkRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::ListGitRepositoryLinksResponse>
  ListGitRepositoryLinks(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::ListGitRepositoryLinksRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::GitRepositoryLink>
  GetGitRepositoryLink(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::GetGitRepositoryLinkRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::FetchReadWriteTokenResponse>
  FetchReadWriteToken(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::FetchReadWriteTokenRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::FetchReadTokenResponse>
  FetchReadToken(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::FetchReadTokenRequest const& request)
      override;

  StatusOr<
      google::cloud::developerconnect::v1::FetchLinkableGitRepositoriesResponse>
  FetchLinkableGitRepositories(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::
          FetchLinkableGitRepositoriesRequest const& request) override;

  StatusOr<
      google::cloud::developerconnect::v1::FetchGitHubInstallationsResponse>
  FetchGitHubInstallations(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::
          FetchGitHubInstallationsRequest const& request) override;

  StatusOr<google::cloud::developerconnect::v1::FetchGitRefsResponse>
  FetchGitRefs(grpc::ClientContext& context, Options const& options,
               google::cloud::developerconnect::v1::FetchGitRefsRequest const&
                   request) override;

  StatusOr<google::cloud::developerconnect::v1::ListAccountConnectorsResponse>
  ListAccountConnectors(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::ListAccountConnectorsRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::AccountConnector>
  GetAccountConnector(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::GetAccountConnectorRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncCreateAccountConnector(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::CreateAccountConnectorRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> CreateAccountConnector(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::CreateAccountConnectorRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncUpdateAccountConnector(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::UpdateAccountConnectorRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> UpdateAccountConnector(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::UpdateAccountConnectorRequest const&
          request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncDeleteAccountConnector(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::DeleteAccountConnectorRequest const&
          request) override;

  StatusOr<google::longrunning::Operation> DeleteAccountConnector(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::DeleteAccountConnectorRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::FetchAccessTokenResponse>
  FetchAccessToken(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::FetchAccessTokenRequest const&
          request) override;

  StatusOr<google::cloud::developerconnect::v1::ListUsersResponse> ListUsers(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::ListUsersRequest const& request)
      override;

  future<StatusOr<google::longrunning::Operation>> AsyncDeleteUser(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::DeleteUserRequest const& request)
      override;

  StatusOr<google::longrunning::Operation> DeleteUser(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::DeleteUserRequest const& request)
      override;

  StatusOr<google::cloud::developerconnect::v1::User> FetchSelf(
      grpc::ClientContext& context, Options const& options,
      google::cloud::developerconnect::v1::FetchSelfRequest const& request)
      override;

  future<StatusOr<google::longrunning::Operation>> AsyncDeleteSelf(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::cloud::developerconnect::v1::DeleteSelfRequest const& request)
      override;

  StatusOr<google::longrunning::Operation> DeleteSelf(
      grpc::ClientContext& context, Options options,
      google::cloud::developerconnect::v1::DeleteSelfRequest const& request)
      override;

  StatusOr<google::cloud::location::ListLocationsResponse> ListLocations(
      grpc::ClientContext& context, Options const& options,
      google::cloud::location::ListLocationsRequest const& request) override;

  StatusOr<google::cloud::location::Location> GetLocation(
      grpc::ClientContext& context, Options const& options,
      google::cloud::location::GetLocationRequest const& request) override;

  StatusOr<google::longrunning::ListOperationsResponse> ListOperations(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::ListOperationsRequest const& request) override;

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::GetOperationRequest const& request) override;

  Status DeleteOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::DeleteOperationRequest const& request) override;

  Status CancelOperation(
      grpc::ClientContext& context, Options const& options,
      google::longrunning::CancelOperationRequest const& request) override;

  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::GetOperationRequest const& request) override;

  future<Status> AsyncCancelOperation(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::longrunning::CancelOperationRequest const& request) override;

 private:
  void SetMetadata(grpc::ClientContext& context, Options const& options,
                   std::string const& request_params);
  void SetMetadata(grpc::ClientContext& context, Options const& options);

  std::shared_ptr<DeveloperConnectStub> child_;
  std::multimap<std::string, std::string> fixed_metadata_;
  std::string api_client_header_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace developerconnect_v1_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DEVELOPERCONNECT_V1_INTERNAL_DEVELOPER_CONNECT_METADATA_DECORATOR_H
