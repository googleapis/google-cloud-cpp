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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_DATA_CLIENT_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_DATA_CLIENT_H_

#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include "bigtable/client/client_options.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the interface to access Cloud Bigtable.
 *
 * This class is used by the Cloud Bigtable wrappers to access Cloud Bigtable.
 * It provides a configuration point to control how we do load-balancing, how we
 * handle reconnections, refresh authorization tokens, etc.
 */
class DataClient {
 public:
  virtual ~DataClient() = default;

  virtual std::string const& project_id() const = 0;
  virtual std::string const& instance_id() const = 0;

  virtual std::shared_ptr<google::bigtable::v2::Bigtable::StubInterface>
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

/// Create the default implementation of ClientInterface.
std::shared_ptr<DataClient> CreateDefaultDataClient(std::string project_id,
                                                    std::string instance_id,
                                                    ClientOptions options);

/**
 * Return the fully qualified instance name for the @p client.
 *
 * Compute the full path of the instance associated with the client, i.e.,
 * `projects/instances/<client->project_id()>/instances/<client->instance_id()>`
 */
inline std::string InstanceName(std::shared_ptr<DataClient> client) {
  return "projects/" + client->project_id() + "/instances/" +
         client->instance_id();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_DATA_CLIENT_H_
