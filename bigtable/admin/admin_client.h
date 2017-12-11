// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_ADMIN_CLIENT_H_

#include "bigtable/client/client_options.h"

#include <memory>
#include <string>

#include <absl/strings/string_view.h>

#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class TableAdmin;

class AdminClient {
 public:
  virtual ~AdminClient() = default;

  /// The project that this AdminClient works on.
  virtual absl::string_view project() const = 0;

  /// Create a new object to manage an specific instance.
  virtual std::unique_ptr<TableAdmin> CreateTableAdmin(std::string instance_id) = 0;

  /**
   * A callback to handle failures in the client.
   *
   * The client may need to update its data structures (e.g. any grpc channels),
   * or refresh the credentials used to contact the server.
   *
   * @param status the grpc error.
   */
  virtual void OnFailure(grpc::Status const& status) = 0;

  /// Return a new stub to handle admin operations.
  virtual ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface&
  table_admin() = 0;
};

/// Create a new admin client configured via @p options.
std::unique_ptr<AdminClient> CreateAdminClient(std::string project,
                                               bigtable::ClientOptions options);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_ADMIN_CLIENT_H_
