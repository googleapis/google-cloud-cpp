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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ADMIN_CLIENT_H_

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/version.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Forward declare some classes so we can be friends.
class TableAdmin;
namespace internal {
class AsyncAwaitConsistency;
class AsyncCheckConsistency;
template <typename Client, typename Response>
class AsyncLongrunningOperation;
}  // namespace internal

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
 *
 * @par Cost
 * Applications should avoid unnecessarily creating new objects of type
 * `AdminClient`. Creating a new object of this type typically requires
 * connecting to the Cloud Bigtable servers, and performing the authentication
 * workflows with Google Cloud Platform. These operations can take many
 * milliseconds, therefore applications should try to reuse the same
 * `AdminClient` instances when possible.
 */
class AdminClient {
 public:
  virtual ~AdminClient() = default;

  /// The project that this AdminClient works on.
  virtual std::string const& project() const = 0;

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
  friend class TableAdmin;
  friend class internal::AsyncAwaitConsistency;
  friend class internal::AsyncCheckConsistency;
  template <typename Client, typename Response>
  friend class internal::AsyncLongrunningOperation;
  //@{
  /// @name The `google.bigtable.admin.v2.TableAdmin` operations.
  virtual grpc::Status CreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      google::bigtable::admin::v2::Table* response) = 0;
  virtual grpc::Status ListTables(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListTablesRequest const& request,
      google::bigtable::admin::v2::ListTablesResponse* response) = 0;
  virtual grpc::Status GetTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetTableRequest const& request,
      google::bigtable::admin::v2::Table* response) = 0;
  virtual std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncGetTable(grpc::ClientContext* context,
                google::bigtable::admin::v2::GetTableRequest const& request,
                grpc::CompletionQueue* cq) = 0;

  virtual grpc::Status DeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      google::protobuf::Empty* response) = 0;
  virtual grpc::Status ModifyColumnFamilies(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
      google::bigtable::admin::v2::Table* response) = 0;
  virtual grpc::Status DropRowRange(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DropRowRangeRequest const& request,
      google::protobuf::Empty* response) = 0;
  virtual grpc::Status GenerateConsistencyToken(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
          request,
      google::bigtable::admin::v2::GenerateConsistencyTokenResponse*
          response) = 0;
  virtual grpc::Status CheckConsistency(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CheckConsistencyRequest const& request,
      google::bigtable::admin::v2::CheckConsistencyResponse* response) = 0;
  //@}

  //@{
  /// @name The `google.longrunning.Operations` wrappers.
  virtual grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) = 0;
  //@}

  //@{
  /// @name The `google.bigtable.admin.v2.TableAdmin` Async operations.
  virtual std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncCreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      grpc::CompletionQueue* cq) = 0;
  virtual std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      grpc::CompletionQueue* cq) = 0;
  virtual std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncModifyColumnFamilies(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
      grpc::CompletionQueue* cq) = 0;
  virtual std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDropRowRange(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DropRowRangeRequest const& request,
      grpc::CompletionQueue* cq) = 0;
  virtual std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>
  AsyncGenerateConsistencyToken(
      grpc::ClientContext* context,
      const google::bigtable::admin::v2::GenerateConsistencyTokenRequest&
          request,
      grpc::CompletionQueue* cq) = 0;
  virtual std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::CheckConsistencyResponse>>
  AsyncCheckConsistency(
      grpc::ClientContext* context,
      const google::bigtable::admin::v2::CheckConsistencyRequest& request,
      grpc::CompletionQueue* cq) = 0;
  virtual std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListTablesResponse>>
  AsyncListTables(grpc::ClientContext* context,
                  google::bigtable::admin::v2::ListTablesRequest const& request,
                  grpc::CompletionQueue* cq) = 0;
  //@}

  //@{
  /// @name The `google.longrunning.Operations` async wrappers.
  virtual std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncGetOperation(grpc::ClientContext* context,
                    const google::longrunning::GetOperationRequest& request,
                    grpc::CompletionQueue* cq) = 0;
  //@}
};

/// Create a new admin client configured via @p options.
std::shared_ptr<AdminClient> CreateDefaultAdminClient(std::string project,
                                                      ClientOptions options);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ADMIN_CLIENT_H_
