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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ADMIN_CLIENT_H_

#include "bigtable/client/client_options.h"

#include <memory>
#include <string>

#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Connects to Cloud Bigtable's table administration APIs.
 *
 * This class is used by the Cloud Bigtable wrappers to access Cloud Bigtable.
 * Multiple `bigtable::TableAdmin` objects may share a connection via a
 * single `AdminClient` object. The `AdminClient` object is configured at
 * construction time, this configuration includes the credentials, access
 * endpoints, default timeouts, and other gRPC configuration options. This is an
 * interface class because it is also used as a dependency injection point in
 * some of the tests.
 */
class AdminClient {
 public:
  virtual ~AdminClient() = default;

  /// The project that this AdminClient works on.
  virtual std::string const& project() const = 0;

  /// Return a new stub to handle admin operations.
  virtual std::shared_ptr<
      ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>
  Stub() = 0;

  /**
   * Reset and create a new Stub().
   *
   * Currently this is only used in testing.  In the future, we expect this,
   * or a similar member function, will be needed to handle errors that require
   * a new connection, or an explicit refresh of the credentials.
   */
  virtual void reset() = 0;

  /**
   * A callback for completed RPCs.
   *
   * Currently this is only used in testing.  In the future, we expect that
   * some errors may require the class to update its state.
   */
  virtual void on_completion(grpc::Status const&) = 0;
};

/// Create a new admin client configured via @p options.
std::shared_ptr<AdminClient> CreateDefaultAdminClient(
    std::string project, bigtable::ClientOptions options);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ADMIN_CLIENT_H_
