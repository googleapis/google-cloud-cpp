// Copyright 2022 Google LLC
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
// source:
// google/cloud/beyondcorp/appconnections/v1/app_connections_service.proto

#include "google/cloud/beyondcorp/appconnections/v1/internal/app_connections_connection_impl.h"
#include "google/cloud/beyondcorp/appconnections/v1/internal/app_connections_option_defaults.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/async_long_running_operation.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/retry_loop.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace beyondcorp_appconnections_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::unique_ptr<beyondcorp_appconnections_v1::AppConnectionsServiceRetryPolicy>
retry_policy(Options const& options) {
  return options
      .get<beyondcorp_appconnections_v1::
               AppConnectionsServiceRetryPolicyOption>()
      ->clone();
}

std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options
      .get<beyondcorp_appconnections_v1::
               AppConnectionsServiceBackoffPolicyOption>()
      ->clone();
}

std::unique_ptr<beyondcorp_appconnections_v1::
                    AppConnectionsServiceConnectionIdempotencyPolicy>
idempotency_policy(Options const& options) {
  return options
      .get<beyondcorp_appconnections_v1::
               AppConnectionsServiceConnectionIdempotencyPolicyOption>()
      ->clone();
}

std::unique_ptr<PollingPolicy> polling_policy(Options const& options) {
  return options
      .get<beyondcorp_appconnections_v1::
               AppConnectionsServicePollingPolicyOption>()
      ->clone();
}

}  // namespace

AppConnectionsServiceConnectionImpl::AppConnectionsServiceConnectionImpl(
    std::unique_ptr<google::cloud::BackgroundThreads> background,
    std::shared_ptr<
        beyondcorp_appconnections_v1_internal::AppConnectionsServiceStub>
        stub,
    Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      options_(internal::MergeOptions(
          std::move(options), AppConnectionsServiceConnection::options())) {}

StreamRange<google::cloud::beyondcorp::appconnections::v1::AppConnection>
AppConnectionsServiceConnectionImpl::ListAppConnections(
    google::cloud::beyondcorp::appconnections::v1::ListAppConnectionsRequest
        request) {
  request.clear_page_token();
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto idempotency = idempotency_policy(*current)->ListAppConnections(request);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<StreamRange<
      google::cloud::beyondcorp::appconnections::v1::AppConnection>>(
      current, std::move(request),
      [idempotency, function_name, stub = stub_,
       retry = std::shared_ptr<
           beyondcorp_appconnections_v1::AppConnectionsServiceRetryPolicy>(
           retry_policy(*current)),
       backoff = std::shared_ptr<BackoffPolicy>(backoff_policy(*current))](
          Options const& options, google::cloud::beyondcorp::appconnections::
                                      v1::ListAppConnectionsRequest const& r) {
        return google::cloud::internal::RetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](grpc::ClientContext& context, Options const& options,
                   google::cloud::beyondcorp::appconnections::v1::
                       ListAppConnectionsRequest const& request) {
              return stub->ListAppConnections(context, options, request);
            },
            options, r, function_name);
      },
      [](google::cloud::beyondcorp::appconnections::v1::
             ListAppConnectionsResponse r) {
        std::vector<
            google::cloud::beyondcorp::appconnections::v1::AppConnection>
            result(r.app_connections().size());
        auto& messages = *r.mutable_app_connections();
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>
AppConnectionsServiceConnectionImpl::GetAppConnection(
    google::cloud::beyondcorp::appconnections::v1::
        GetAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->GetAppConnection(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::cloud::beyondcorp::appconnections::v1::
                 GetAppConnectionRequest const& request) {
        return stub_->GetAppConnection(context, options, request);
      },
      *current, request, __func__);
}

future<StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>>
AppConnectionsServiceConnectionImpl::CreateAppConnection(
    google::cloud::beyondcorp::appconnections::v1::
        CreateAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto request_copy = request;
  auto const idempotent =
      idempotency_policy(*current)->CreateAppConnection(request_copy);
  return google::cloud::internal::AsyncLongRunningOperation<
      google::cloud::beyondcorp::appconnections::v1::AppConnection>(
      background_->cq(), current, std::move(request_copy),
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::cloud::beyondcorp::appconnections::v1::
                         CreateAppConnectionRequest const& request) {
        return stub->AsyncCreateAppConnection(cq, std::move(context),
                                              std::move(options), request);
      },
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::longrunning::GetOperationRequest const& request) {
        return stub->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [stub = stub_](
          google::cloud::CompletionQueue& cq,
          std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::longrunning::CancelOperationRequest const& request) {
        return stub->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      &google::cloud::internal::ExtractLongRunningResultResponse<
          google::cloud::beyondcorp::appconnections::v1::AppConnection>,
      retry_policy(*current), backoff_policy(*current), idempotent,
      polling_policy(*current), __func__);
}

StatusOr<google::longrunning::Operation>
AppConnectionsServiceConnectionImpl::CreateAppConnection(
    NoAwaitTag, google::cloud::beyondcorp::appconnections::v1::
                    CreateAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->CreateAppConnection(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::cloud::beyondcorp::appconnections::v1::
                 CreateAppConnectionRequest const& request) {
        return stub_->CreateAppConnection(context, options, request);
      },
      *current, request, __func__);
}

future<StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>>
AppConnectionsServiceConnectionImpl::CreateAppConnection(
    google::longrunning::Operation const& operation) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  if (!operation.metadata()
           .Is<typename google::cloud::beyondcorp::appconnections::v1::
                   AppConnectionOperationMetadata>()) {
    return make_ready_future<
        StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>>(
        internal::InvalidArgumentError(
            "operation does not correspond to CreateAppConnection",
            GCP_ERROR_INFO().WithMetadata("operation",
                                          operation.metadata().DebugString())));
  }

  return google::cloud::internal::AsyncAwaitLongRunningOperation<
      google::cloud::beyondcorp::appconnections::v1::AppConnection>(
      background_->cq(), current, operation,
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::longrunning::GetOperationRequest const& request) {
        return stub->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [stub = stub_](
          google::cloud::CompletionQueue& cq,
          std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::longrunning::CancelOperationRequest const& request) {
        return stub->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      &google::cloud::internal::ExtractLongRunningResultResponse<
          google::cloud::beyondcorp::appconnections::v1::AppConnection>,
      polling_policy(*current), __func__);
}

future<StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>>
AppConnectionsServiceConnectionImpl::UpdateAppConnection(
    google::cloud::beyondcorp::appconnections::v1::
        UpdateAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto request_copy = request;
  auto const idempotent =
      idempotency_policy(*current)->UpdateAppConnection(request_copy);
  return google::cloud::internal::AsyncLongRunningOperation<
      google::cloud::beyondcorp::appconnections::v1::AppConnection>(
      background_->cq(), current, std::move(request_copy),
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::cloud::beyondcorp::appconnections::v1::
                         UpdateAppConnectionRequest const& request) {
        return stub->AsyncUpdateAppConnection(cq, std::move(context),
                                              std::move(options), request);
      },
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::longrunning::GetOperationRequest const& request) {
        return stub->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [stub = stub_](
          google::cloud::CompletionQueue& cq,
          std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::longrunning::CancelOperationRequest const& request) {
        return stub->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      &google::cloud::internal::ExtractLongRunningResultResponse<
          google::cloud::beyondcorp::appconnections::v1::AppConnection>,
      retry_policy(*current), backoff_policy(*current), idempotent,
      polling_policy(*current), __func__);
}

StatusOr<google::longrunning::Operation>
AppConnectionsServiceConnectionImpl::UpdateAppConnection(
    NoAwaitTag, google::cloud::beyondcorp::appconnections::v1::
                    UpdateAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->UpdateAppConnection(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::cloud::beyondcorp::appconnections::v1::
                 UpdateAppConnectionRequest const& request) {
        return stub_->UpdateAppConnection(context, options, request);
      },
      *current, request, __func__);
}

future<StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>>
AppConnectionsServiceConnectionImpl::UpdateAppConnection(
    google::longrunning::Operation const& operation) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  if (!operation.metadata()
           .Is<typename google::cloud::beyondcorp::appconnections::v1::
                   AppConnectionOperationMetadata>()) {
    return make_ready_future<
        StatusOr<google::cloud::beyondcorp::appconnections::v1::AppConnection>>(
        internal::InvalidArgumentError(
            "operation does not correspond to UpdateAppConnection",
            GCP_ERROR_INFO().WithMetadata("operation",
                                          operation.metadata().DebugString())));
  }

  return google::cloud::internal::AsyncAwaitLongRunningOperation<
      google::cloud::beyondcorp::appconnections::v1::AppConnection>(
      background_->cq(), current, operation,
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::longrunning::GetOperationRequest const& request) {
        return stub->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [stub = stub_](
          google::cloud::CompletionQueue& cq,
          std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::longrunning::CancelOperationRequest const& request) {
        return stub->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      &google::cloud::internal::ExtractLongRunningResultResponse<
          google::cloud::beyondcorp::appconnections::v1::AppConnection>,
      polling_policy(*current), __func__);
}

future<StatusOr<google::cloud::beyondcorp::appconnections::v1::
                    AppConnectionOperationMetadata>>
AppConnectionsServiceConnectionImpl::DeleteAppConnection(
    google::cloud::beyondcorp::appconnections::v1::
        DeleteAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto request_copy = request;
  auto const idempotent =
      idempotency_policy(*current)->DeleteAppConnection(request_copy);
  return google::cloud::internal::AsyncLongRunningOperation<
      google::cloud::beyondcorp::appconnections::v1::
          AppConnectionOperationMetadata>(
      background_->cq(), current, std::move(request_copy),
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::cloud::beyondcorp::appconnections::v1::
                         DeleteAppConnectionRequest const& request) {
        return stub->AsyncDeleteAppConnection(cq, std::move(context),
                                              std::move(options), request);
      },
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::longrunning::GetOperationRequest const& request) {
        return stub->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [stub = stub_](
          google::cloud::CompletionQueue& cq,
          std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::longrunning::CancelOperationRequest const& request) {
        return stub->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      &google::cloud::internal::ExtractLongRunningResultMetadata<
          google::cloud::beyondcorp::appconnections::v1::
              AppConnectionOperationMetadata>,
      retry_policy(*current), backoff_policy(*current), idempotent,
      polling_policy(*current), __func__);
}

StatusOr<google::longrunning::Operation>
AppConnectionsServiceConnectionImpl::DeleteAppConnection(
    NoAwaitTag, google::cloud::beyondcorp::appconnections::v1::
                    DeleteAppConnectionRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->DeleteAppConnection(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::cloud::beyondcorp::appconnections::v1::
                 DeleteAppConnectionRequest const& request) {
        return stub_->DeleteAppConnection(context, options, request);
      },
      *current, request, __func__);
}

future<StatusOr<google::cloud::beyondcorp::appconnections::v1::
                    AppConnectionOperationMetadata>>
AppConnectionsServiceConnectionImpl::DeleteAppConnection(
    google::longrunning::Operation const& operation) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  if (!operation.metadata()
           .Is<typename google::cloud::beyondcorp::appconnections::v1::
                   AppConnectionOperationMetadata>()) {
    return make_ready_future<
        StatusOr<google::cloud::beyondcorp::appconnections::v1::
                     AppConnectionOperationMetadata>>(
        internal::InvalidArgumentError(
            "operation does not correspond to DeleteAppConnection",
            GCP_ERROR_INFO().WithMetadata("operation",
                                          operation.metadata().DebugString())));
  }

  return google::cloud::internal::AsyncAwaitLongRunningOperation<
      google::cloud::beyondcorp::appconnections::v1::
          AppConnectionOperationMetadata>(
      background_->cq(), current, operation,
      [stub = stub_](google::cloud::CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::longrunning::GetOperationRequest const& request) {
        return stub->AsyncGetOperation(cq, std::move(context),
                                       std::move(options), request);
      },
      [stub = stub_](
          google::cloud::CompletionQueue& cq,
          std::shared_ptr<grpc::ClientContext> context,
          google::cloud::internal::ImmutableOptions options,
          google::longrunning::CancelOperationRequest const& request) {
        return stub->AsyncCancelOperation(cq, std::move(context),
                                          std::move(options), request);
      },
      &google::cloud::internal::ExtractLongRunningResultMetadata<
          google::cloud::beyondcorp::appconnections::v1::
              AppConnectionOperationMetadata>,
      polling_policy(*current), __func__);
}

StreamRange<google::cloud::beyondcorp::appconnections::v1::
                ResolveAppConnectionsResponse::AppConnectionDetails>
AppConnectionsServiceConnectionImpl::ResolveAppConnections(
    google::cloud::beyondcorp::appconnections::v1::ResolveAppConnectionsRequest
        request) {
  request.clear_page_token();
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto idempotency =
      idempotency_policy(*current)->ResolveAppConnections(request);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<
      StreamRange<google::cloud::beyondcorp::appconnections::v1::
                      ResolveAppConnectionsResponse::AppConnectionDetails>>(
      current, std::move(request),
      [idempotency, function_name, stub = stub_,
       retry = std::shared_ptr<
           beyondcorp_appconnections_v1::AppConnectionsServiceRetryPolicy>(
           retry_policy(*current)),
       backoff = std::shared_ptr<BackoffPolicy>(backoff_policy(*current))](
          Options const& options,
          google::cloud::beyondcorp::appconnections::v1::
              ResolveAppConnectionsRequest const& r) {
        return google::cloud::internal::RetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](grpc::ClientContext& context, Options const& options,
                   google::cloud::beyondcorp::appconnections::v1::
                       ResolveAppConnectionsRequest const& request) {
              return stub->ResolveAppConnections(context, options, request);
            },
            options, r, function_name);
      },
      [](google::cloud::beyondcorp::appconnections::v1::
             ResolveAppConnectionsResponse r) {
        std::vector<google::cloud::beyondcorp::appconnections::v1::
                        ResolveAppConnectionsResponse::AppConnectionDetails>
            result(r.app_connection_details().size());
        auto& messages = *r.mutable_app_connection_details();
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

StreamRange<google::cloud::location::Location>
AppConnectionsServiceConnectionImpl::ListLocations(
    google::cloud::location::ListLocationsRequest request) {
  request.clear_page_token();
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto idempotency = idempotency_policy(*current)->ListLocations(request);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<
      StreamRange<google::cloud::location::Location>>(
      current, std::move(request),
      [idempotency, function_name, stub = stub_,
       retry = std::shared_ptr<
           beyondcorp_appconnections_v1::AppConnectionsServiceRetryPolicy>(
           retry_policy(*current)),
       backoff = std::shared_ptr<BackoffPolicy>(backoff_policy(*current))](
          Options const& options,
          google::cloud::location::ListLocationsRequest const& r) {
        return google::cloud::internal::RetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](
                grpc::ClientContext& context, Options const& options,
                google::cloud::location::ListLocationsRequest const& request) {
              return stub->ListLocations(context, options, request);
            },
            options, r, function_name);
      },
      [](google::cloud::location::ListLocationsResponse r) {
        std::vector<google::cloud::location::Location> result(
            r.locations().size());
        auto& messages = *r.mutable_locations();
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

StatusOr<google::cloud::location::Location>
AppConnectionsServiceConnectionImpl::GetLocation(
    google::cloud::location::GetLocationRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->GetLocation(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::cloud::location::GetLocationRequest const& request) {
        return stub_->GetLocation(context, options, request);
      },
      *current, request, __func__);
}

StatusOr<google::iam::v1::Policy>
AppConnectionsServiceConnectionImpl::SetIamPolicy(
    google::iam::v1::SetIamPolicyRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->SetIamPolicy(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::iam::v1::SetIamPolicyRequest const& request) {
        return stub_->SetIamPolicy(context, options, request);
      },
      *current, request, __func__);
}

StatusOr<google::iam::v1::Policy>
AppConnectionsServiceConnectionImpl::GetIamPolicy(
    google::iam::v1::GetIamPolicyRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->GetIamPolicy(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::iam::v1::GetIamPolicyRequest const& request) {
        return stub_->GetIamPolicy(context, options, request);
      },
      *current, request, __func__);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
AppConnectionsServiceConnectionImpl::TestIamPermissions(
    google::iam::v1::TestIamPermissionsRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->TestIamPermissions(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::iam::v1::TestIamPermissionsRequest const& request) {
        return stub_->TestIamPermissions(context, options, request);
      },
      *current, request, __func__);
}

StreamRange<google::longrunning::Operation>
AppConnectionsServiceConnectionImpl::ListOperations(
    google::longrunning::ListOperationsRequest request) {
  request.clear_page_token();
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto idempotency = idempotency_policy(*current)->ListOperations(request);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<
      StreamRange<google::longrunning::Operation>>(
      current, std::move(request),
      [idempotency, function_name, stub = stub_,
       retry = std::shared_ptr<
           beyondcorp_appconnections_v1::AppConnectionsServiceRetryPolicy>(
           retry_policy(*current)),
       backoff = std::shared_ptr<BackoffPolicy>(backoff_policy(*current))](
          Options const& options,
          google::longrunning::ListOperationsRequest const& r) {
        return google::cloud::internal::RetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](grpc::ClientContext& context, Options const& options,
                   google::longrunning::ListOperationsRequest const& request) {
              return stub->ListOperations(context, options, request);
            },
            options, r, function_name);
      },
      [](google::longrunning::ListOperationsResponse r) {
        std::vector<google::longrunning::Operation> result(
            r.operations().size());
        auto& messages = *r.mutable_operations();
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

StatusOr<google::longrunning::Operation>
AppConnectionsServiceConnectionImpl::GetOperation(
    google::longrunning::GetOperationRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->GetOperation(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::longrunning::GetOperationRequest const& request) {
        return stub_->GetOperation(context, options, request);
      },
      *current, request, __func__);
}

Status AppConnectionsServiceConnectionImpl::DeleteOperation(
    google::longrunning::DeleteOperationRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->DeleteOperation(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::longrunning::DeleteOperationRequest const& request) {
        return stub_->DeleteOperation(context, options, request);
      },
      *current, request, __func__);
}

Status AppConnectionsServiceConnectionImpl::CancelOperation(
    google::longrunning::CancelOperationRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->CancelOperation(request),
      [this](grpc::ClientContext& context, Options const& options,
             google::longrunning::CancelOperationRequest const& request) {
        return stub_->CancelOperation(context, options, request);
      },
      *current, request, __func__);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace beyondcorp_appconnections_v1_internal
}  // namespace cloud
}  // namespace google
