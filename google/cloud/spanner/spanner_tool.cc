#include <iostream>
#include <memory>
#include <grpcpp/grpcpp.h>
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>

int main() {
  namespace spanner = google::spanner::admin::database::v1;
  auto const cred = grpc::GoogleDefaultCredentials();
  auto const channel = grpc::CreateChannel("spanner.googleapis.com", cred);
  std::unique_ptr<spanner::DatabaseAdmin::Stub> stub(
      spanner::DatabaseAdmin::NewStub(channel));

  spanner::ListDatabasesResponse response;
  spanner::ListDatabasesRequest request;
  request.set_parent("projects/jgm-cloud-cxx/instances/jgm-spanner-instance");

  grpc::ClientContext context;
  auto status = stub->ListDatabases(&context, request, &response);

  if (!status.ok()) {
    std::cerr << "FAILED: " << status.error_code() << ": "
              << status.error_message() << "\n";
  }

  std::cout << "Response:\n";
  std::cout << response.DebugString() << "\n";
}
