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

// This is a command-line tool to let folks easily experiment with Spanner
// using C++. This works with bazel using a command like:
//
// $ bazel run google/cloud/spanner:spanner_tool -- \
//       jgm-cloud-cxx jgm-spanner-instance
//
// Currently, the above command will just invoke the "ListDatabases" RPC, which
// makes it equivalent to the following command:
//
// $ gcloud spanner databases list \
//       --project jgm-cloud-cxx --instance jgm-spanner-instance
//
// NOTE: The actual project and instance names will vary for other users; These
// are just examples.
int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: spanner_tool <project> <instance>\n";
    return 1;
  }

  ListDatabases(argv[1], argv[2]);
}
