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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class DatabaseAdminClient {
 public:
  explicit DatabaseAdminClient(
      ClientOptions const& client_options = ClientOptions());

  future<StatusOr<google::spanner::admin::database::v1::Database>>
  CreateDatabase(std::string const& project_id, std::string const& instance_id,
                 std::string const& database_id,
                 std::vector<std::string> const& extra_statements = {});

  Status DropDatabase(std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& database_id);

 private:
  std::shared_ptr<internal::DatabaseAdminStub> stub_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATABASE_ADMIN_CLIENT_H_
