#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

namespace {

void ListDatabases(std::string const& project, std::string const& instance) {
  namespace spanner = google::spanner::admin::database::v1;

  std::shared_ptr<grpc::ChannelCredentials> cred =
      grpc::GoogleDefaultCredentials();
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("spanner.googleapis.com", std::move(cred));
  std::unique_ptr<spanner::DatabaseAdmin::Stub> stub(
      spanner::DatabaseAdmin::NewStub(std::move(channel)));

  spanner::ListDatabasesResponse response;
  spanner::ListDatabasesRequest request;
  request.set_parent("projects/" + project + "/instances/" + instance);

  grpc::ClientContext context;
  grpc::Status status = stub->ListDatabases(&context, request, &response);

  if (!status.ok()) {
    std::cerr << "FAILED: " << status.error_code() << ": "
              << status.error_message() << "\n";
    return;
  }

  std::cout << "Response:\n";
  std::cout << response.DebugString() << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: spanner_tool <project> <instance>\n";
    return 1;
  }

  ListDatabases(argv[1], argv[2]);
}
