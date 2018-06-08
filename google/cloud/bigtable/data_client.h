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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_DATA_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_DATA_CLIENT_H_

#include "google/cloud/bigtable/client_options.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Forward declare some classes so we can be friends.
class Table;
namespace noex {
class Table;
}  // namespace noex
namespace internal {
class BulkMutator;
}  // namespace internal

/**
 * Connects to Cloud Bigtable's data manipulation APIs.
 *
 * This class is used by the Cloud Bigtable wrappers to access Cloud Bigtable.
 * Multiple `bigtable::Table` objects may share a connection via a single
 * `DataClient` object. The `DataClient` object is configured at construction
 * time, this configuration includes the credentials, access endpoints, default
 * timeouts, and other gRPC configuration options. This is an interface class
 * because it is also used as a dependency injection point in some of the tests.
 */
class DataClient {
 public:
  virtual ~DataClient() = default;

  virtual std::string const& project_id() const = 0;
  virtual std::string const& instance_id() const = 0;

  /**
   * Return a new channel to handle admin operations.
   *
   * Intended to access rarely used services in the same endpoints as the
   * Bigtable admin interfaces, for example, the google.longrunning.Operations.
   */
  virtual std::shared_ptr<grpc::Channel> Channel() = 0;

  /**
   * Reset and create new Channels.
   *
   * Currently this is only used in testing.  In the future, we expect this,
   * or a similar member function, will be needed to handle errors that require
   * a new connection, or an explicit refresh of the credentials.
   */
  virtual void reset() = 0;

  // The member functions of this class are not intended for general use by
  // application developers (they are simply a dependency injection point). Make
  // them protected, so the mock classes can override them, and then make the
  // classes that do use them friends.
 protected:
  friend class Table;
  friend class noex::Table;
  friend class internal::BulkMutator;
  friend class RowReader;
  //@{
  /// @name the `google.bigtable.v2.Bigtable` wrappers.
  virtual grpc::Status MutateRow(
      grpc::ClientContext* context,
      google::bigtable::v2::MutateRowRequest const& request,
      google::bigtable::v2::MutateRowResponse* response) = 0;
  virtual grpc::Status CheckAndMutateRow(
      grpc::ClientContext* context,
      google::bigtable::v2::CheckAndMutateRowRequest const& request,
      google::bigtable::v2::CheckAndMutateRowResponse* response) = 0;
  virtual grpc::Status ReadModifyWriteRow(
      grpc::ClientContext* context,
      google::bigtable::v2::ReadModifyWriteRowRequest const& request,
      google::bigtable::v2::ReadModifyWriteRowResponse* response) = 0;
  virtual std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::ReadRowsResponse>>
  ReadRows(grpc::ClientContext* context,
           google::bigtable::v2::ReadRowsRequest const& request) = 0;
  virtual std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::SampleRowKeysResponse>>
  SampleRowKeys(grpc::ClientContext* context,
                google::bigtable::v2::SampleRowKeysRequest const& request) = 0;
  virtual std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::MutateRowsResponse>>
  MutateRows(grpc::ClientContext* context,
             google::bigtable::v2::MutateRowsRequest const& request) = 0;
  //@}
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
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_DATA_CLIENT_H_
