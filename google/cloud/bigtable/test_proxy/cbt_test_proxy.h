// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TEST_PROXY_CBT_TEST_PROXY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TEST_PROXY_CBT_TEST_PROXY_H

#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/version.h"
#include <google/cloud/bigtable/test_proxy/test_proxy.grpc.pb.h>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace test_proxy {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// A proxy implementing the CloudBigtableV2TestProxy API service using the C++
// data client.
class CbtTestProxy final
    : public ::google::bigtable::testproxy::CloudBigtableV2TestProxy::Service {
 public:
  CbtTestProxy() = default;

  ~CbtTestProxy() override = default;

  grpc::Status CreateClient(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::CreateClientRequest const* request,
      ::google::bigtable::testproxy::CreateClientResponse* response) override;

  grpc::Status CloseClient(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::CloseClientRequest const* request,
      ::google::bigtable::testproxy::CloseClientResponse* response) override;

  grpc::Status RemoveClient(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::RemoveClientRequest const* request,
      ::google::bigtable::testproxy::RemoveClientResponse* response) override;

  grpc::Status ReadRow(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::ReadRowRequest const* request,
      ::google::bigtable::testproxy::RowResult* response) override;

  grpc::Status ReadRows(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::ReadRowsRequest const* request,
      ::google::bigtable::testproxy::RowsResult* response) override;

  grpc::Status MutateRow(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::MutateRowRequest const* request,
      ::google::bigtable::testproxy::MutateRowResult* response) override;

  // Return status reflects if the client binding was successful.
  // Mutation failure information, if any, is in the MutateRowsResult response.
  grpc::Status BulkMutateRows(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::MutateRowsRequest const* request,
      ::google::bigtable::testproxy::MutateRowsResult* response) override;

  grpc::Status CheckAndMutateRow(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::CheckAndMutateRowRequest const* request,
      ::google::bigtable::testproxy::CheckAndMutateRowResult* response)
      override;

  grpc::Status SampleRowKeys(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::SampleRowKeysRequest const* request,
      ::google::bigtable::testproxy::SampleRowKeysResult* response) override;

  grpc::Status ReadModifyWriteRow(
      ::grpc::ServerContext* context,
      ::google::bigtable::testproxy::ReadModifyWriteRowRequest const* request,
      ::google::bigtable::testproxy::RowResult* response) override;

 private:
  StatusOr<std::shared_ptr<DataConnection>> GetConnection(
      std::string const& client_id);

  StatusOr<Table> GetTableFromRequest(std::string const& client_id,
                                      std::string const& table_name);

  std::unordered_map<std::string, std::shared_ptr<DataConnection>> connections_;
  std::mutex mu_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace test_proxy
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TEST_PROXY_CBT_TEST_PROXY_H
