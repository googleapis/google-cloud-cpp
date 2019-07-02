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

#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/internal/database_admin_retry.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
// TODO(#126) - refactor to some common place. Note that this different from
// the MakeStatusFromGrpcError function,
namespace {
StatusCode MapStatusCode(std::int32_t code) {
  if (code < 0 || code > static_cast<std::int32_t>(StatusCode::kDataLoss)) {
    return StatusCode::kUnknown;
  }
  return static_cast<StatusCode>(code);
}
}  // namespace

google::cloud::Status ToGoogleCloudStatus(google::rpc::Status const& status) {
  return google::cloud::Status(MapStatusCode(status.code()), status.message());
}

namespace gcsa = google::spanner::admin::database::v1;

DatabaseAdminClient::DatabaseAdminClient(ClientOptions const& client_options)
    : stub_(new internal::DatabaseAdminRetry(
          internal::CreateDefaultDatabaseAdminStub(client_options))) {}

future<StatusOr<gcsa::Database>> DatabaseAdminClient::CreateDatabase(
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id,
    std::vector<std::string> const& extra_statements) {
  grpc::ClientContext context;
  gcsa::CreateDatabaseRequest request;
  request.set_parent("projects/" + project_id + "/instances/" + instance_id);
  request.set_create_statement("CREATE DATABASE `" + database_id + "`");
  for (auto const& s : extra_statements) {
    *request.add_extra_statements() = s;
  }
  auto operation = stub_->CreateDatabase(context, request);
  if (!operation) {
    return google::cloud::make_ready_future(
        StatusOr<gcsa::Database>(operation.status()));
  }

  // TODO(#127) - use the (implicit) completion queue to run this loop.
  struct Polling {
    google::cloud::promise<StatusOr<gcsa::Database>> promise;
    google::longrunning::Operation operation;
    std::chrono::seconds wait_time = std::chrono::seconds(2);
  };
  Polling polling_state;
  polling_state.operation = *std::move(operation);

  auto f = polling_state.promise.get_future();

  // TODO(#128) - introduce a polling policy to control the polling loop.
  std::thread t(
      [](std::shared_ptr<internal::DatabaseAdminStub> stub,
         Polling ps) mutable {
        auto deadline =
            std::chrono::system_clock::now() + std::chrono::minutes(30);
        while (!ps.operation.done() &&
               std::chrono::system_clock::now() < deadline) {
          auto remaining = deadline - std::chrono::system_clock::now();
          auto sleep = (std::min)(
              remaining,
              std::chrono::duration_cast<decltype(remaining)>(ps.wait_time));
          std::this_thread::sleep_for(sleep);
          grpc::ClientContext poll_context;
          google::longrunning::GetOperationRequest poll_request;
          poll_request.set_name(ps.operation.name());
          auto update = stub->GetOperation(poll_context, poll_request);
          if (!update) {
            stub.reset();
            ps.promise.set_value(update.status());
            return;
          }
          using std::swap;
          swap(*update, ps.operation);
          // Increase the wait time.
          ps.wait_time *= 2;
        }
        if (ps.operation.done()) {
          if (ps.operation.has_error()) {
            // The long running operation failed, return the error to the
            // caller.
            stub.reset();
            ps.promise.set_value(ToGoogleCloudStatus(ps.operation.error()));
            return;
          }
          if (!ps.operation.has_response()) {
            stub.reset();
            ps.promise.set_value(Status(StatusCode::kUnknown,
                                        "CreateDatabase operation completed "
                                        "without error or response, name=" +
                                            ps.operation.name()));
            return;
          }
          google::protobuf::Any const& any = ps.operation.response();
          if (!any.Is<gcsa::Database>()) {
            stub.reset();
            ps.promise.set_value(Status(StatusCode::kInternal,
                                        "CreateDatabase operation completed "
                                        "with an invalid response type, name=" +
                                            ps.operation.name()));
            return;
          }
          gcsa::Database result;
          any.UnpackTo(&result);
          stub.reset();
          ps.promise.set_value(std::move(result));
          return;
        }
        stub.reset();
        ps.promise.set_value(Status(StatusCode::kDeadlineExceeded,
                                    "Deadline exceeded in longrunning "
                                    "operation for CreateDatabase, name=" +
                                        ps.operation.name()));
      },
      stub_, std::move(polling_state));
  t.detach();

  return f;
}

Status DatabaseAdminClient::DropDatabase(std::string const& project_id,
                                         std::string const& instance_id,
                                         std::string const& database_id) {
  grpc::ClientContext context;
  google::spanner::admin::database::v1::DropDatabaseRequest request;
  request.set_database("projects/" + project_id + "/instances/" + instance_id +
                       "/databases/" + database_id);

  return stub_->DropDatabase(context, request);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
