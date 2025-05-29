// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/emulator/server.h"
#include "google/cloud/bigtable/emulator/cluster.h"
#include "google/cloud/bigtable/emulator/to_grpc_status.h"
#include "google/cloud/internal/make_status.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/protobuf/util/time_util.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <grpcpp/impl/call_op_set.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/status.h>
#include <cstddef>
#include <cstdint>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

namespace btproto = ::google::bigtable::v2;
namespace btadmin = ::google::bigtable::admin::v2;

class EmulatorService final : public btproto::Bigtable::Service {
 public:
  explicit EmulatorService(std::shared_ptr<Cluster> cluster)
      : cluster_(std::move(cluster)) {}

  grpc::Status ReadRows(
      grpc::ServerContext* /* context */,
      btproto::ReadRowsRequest const* request,
      grpc::ServerWriter<btproto::ReadRowsResponse>* writer) override {
    auto maybe_table = cluster_->FindTable(request->table_name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }
    RowStreamer row_streamer(*writer);
    return ToGrpcStatus((*maybe_table)->ReadRows(*request, row_streamer));
  }

  grpc::Status SampleRowKeys(
      grpc::ServerContext* /* context */,
      btproto::SampleRowKeysRequest const* request,
      grpc::ServerWriter<btproto::SampleRowKeysResponse>* writer) override {
    auto maybe_table = cluster_->FindTable(request->table_name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }

    auto& table = maybe_table.value();

    return ToGrpcStatus(table->SampleRowKeys(0.0001, writer));
  }

  grpc::Status MutateRow(grpc::ServerContext* /* context */,
                         btproto::MutateRowRequest const* request,
                         btproto::MutateRowResponse* /* response */) override {
    auto maybe_table = cluster_->FindTable(request->table_name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }
    return ToGrpcStatus((*maybe_table)->MutateRow(*request));
  }

  grpc::Status MutateRows(
      grpc::ServerContext* /* context */,
      btproto::MutateRowsRequest const* request,
      grpc::ServerWriter<btproto::MutateRowsResponse>* writer) override {
    auto maybe_table = cluster_->FindTable(request->table_name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }

    int64_t index = 0;
    google::bigtable::v2::MutateRowsResponse response;

    for (auto const& entry : request->entries()) {
      response.Clear();

      auto status = (*maybe_table)
                        ->DoMutationsWithPossibleRollbackLocked(
                            entry.row_key(), entry.mutations());

      auto* response_entry = response.add_entries();
      response_entry->set_index(index++);
      auto* s = response_entry->mutable_status();
      *s = ToGoogleRPCStatus(status);

      if (index == request->entries_size()) {
        auto opts = grpc::WriteOptions();
        opts.set_last_message();
        writer->WriteLast(response, opts);
      } else {
        writer->Write(response);
      }
    }

    return grpc::Status::OK;
  }

  grpc::Status CheckAndMutateRow(
      grpc::ServerContext* /* context */,
      btproto::CheckAndMutateRowRequest const* request,
      btproto::CheckAndMutateRowResponse* response) override {
    auto maybe_table = cluster_->FindTable(request->table_name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }

    auto maybe_response = (*maybe_table)->CheckAndMutateRow(*request);
    if (!maybe_response.ok()) {
      return ToGrpcStatus(maybe_response.status());
    }

    *response = std::move(maybe_response.value());

    return grpc::Status::OK;
  }

  grpc::Status PingAndWarm(
      grpc::ServerContext* /* context */,
      btproto::PingAndWarmRequest const* /* request */,
      btproto::PingAndWarmResponse* /* response */) override {
    return grpc::Status::OK;
  }

  grpc::Status ReadModifyWriteRow(
      grpc::ServerContext* /* context */,
      btproto::ReadModifyWriteRowRequest const* /* request */,
      btproto::ReadModifyWriteRowResponse* /* response */) override {
    return grpc::Status::OK;
  }

 private:
  std::shared_ptr<Cluster> cluster_;
};

class EmulatorTableService final : public btadmin::BigtableTableAdmin::Service {
 public:
  explicit EmulatorTableService(std::shared_ptr<Cluster> cluster)
      : cluster_(std::move(cluster)) {}
  grpc::Status CreateTable(grpc::ServerContext* /* context */,
                           btadmin::CreateTableRequest const* request,
                           btadmin::Table* response) override {
    auto table_name = request->parent() + "/tables/" + request->table_id();
    auto maybe_table = cluster_->CreateTable(table_name, request->table());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }
    *response = *std::move(maybe_table);
    return grpc::Status::OK;
  }

  grpc::Status ListTables(grpc::ServerContext* /* context */,
                          btadmin::ListTablesRequest const* request,
                          btadmin::ListTablesResponse* response) override {
    if (!request->page_token().empty()) {
      return ToGrpcStatus(UnimplementedError(
          "Pagination is not supported.",
          GCP_ERROR_INFO().WithMetadata("page_token", request->page_token())));
    }
    auto maybe_tables =
        cluster_->ListTables(request->parent(), request->view());
    if (!maybe_tables) {
      return ToGrpcStatus(maybe_tables.status());
    }
    if (request->page_size() < 0) {
      return ToGrpcStatus(InvalidArgumentError(
          "Negative page size.",
          GCP_ERROR_INFO().WithMetadata("page_size",
                                        std::to_string(request->page_size()))));
    }
    if (request->page_size() > 0 &&
        maybe_tables->size() > static_cast<size_t>(request->page_size())) {
      response->set_next_page_token("unsupported");
      maybe_tables->resize(request->page_size());
    }
    for (auto& table : *maybe_tables) {
      *response->add_tables() = std::move(table);
    }
    return grpc::Status::OK;
  }

  grpc::Status GetTable(grpc::ServerContext* /* context */,
                        btadmin::GetTableRequest const* request,
                        btadmin::Table* response) override {
    auto maybe_table = cluster_->GetTable(request->name(), request->view());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }
    *response = *std::move(maybe_table);
    return grpc::Status::OK;
  }

  grpc::Status UpdateTable(grpc::ServerContext* /* context */,
                           btadmin::UpdateTableRequest const* request,
                           google::longrunning::Operation* response) override {
    auto maybe_table = cluster_->FindTable(request->table().name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }
    auto status =
        (*maybe_table)->Update(request->table(), request->update_mask());
    if (!status.ok()) {
      return ToGrpcStatus(status);
    }
    btadmin::UpdateTableMetadata res_md;
    res_md.set_name(request->table().name());
    *res_md.mutable_start_time() =
        google::protobuf::util::TimeUtil::GetCurrentTime();
    *res_md.mutable_end_time() =
        google::protobuf::util::TimeUtil::GetCurrentTime();
    response->set_name("UpdateTable");
    response->mutable_metadata()->PackFrom(std::move(res_md));
    response->set_done(true);
    google::protobuf::Empty empty_response;
    response->mutable_response()->PackFrom(std::move(empty_response));
    return grpc::Status::OK;
  }

  grpc::Status DeleteTable(grpc::ServerContext* /* context */,
                           btadmin::DeleteTableRequest const* request,
                           google::protobuf::Empty* /* response */) override {
    return ToGrpcStatus(cluster_->DeleteTable(request->name()));
  }

  grpc::Status ModifyColumnFamilies(
      grpc::ServerContext* /* context */,
      btadmin::ModifyColumnFamiliesRequest const* request,
      btadmin::Table* response) override {
    auto maybe_table = cluster_->FindTable(request->name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }
    auto maybe_table_res = (*maybe_table)->ModifyColumnFamilies(*request);
    if (!maybe_table_res) {
      return ToGrpcStatus(maybe_table_res.status());
    }
    *response = *std::move(maybe_table_res);

    return grpc::Status::OK;
  }

  grpc::Status DropRowRange(grpc::ServerContext* /* context */,
                            btadmin::DropRowRangeRequest const* request,
                            google::protobuf::Empty* /* response */) override {
    auto maybe_table = cluster_->FindTable(request->name());
    if (!maybe_table) {
      return ToGrpcStatus(maybe_table.status());
    }

    auto status = (*maybe_table)->DropRowRange(*request);
    if (!status.ok()) {
      return ToGrpcStatus(status);
    }

    return grpc::Status::OK;
  }

  grpc::Status GenerateConsistencyToken(
      grpc::ServerContext* /* context */,
      btadmin::GenerateConsistencyTokenRequest const* request,
      btadmin::GenerateConsistencyTokenResponse* response) override {
    if (!cluster_->HasTable(request->name())) {
      return ToGrpcStatus(NotFoundError(
          "Table does not exist.",
          GCP_ERROR_INFO().WithMetadata("table_name", request->name())));
    }
    response->set_consistency_token("some fake token");
    return grpc::Status::OK;
  }

  grpc::Status CheckConsistency(
      grpc::ServerContext* /* context */,
      btadmin::CheckConsistencyRequest const* request,
      btadmin::CheckConsistencyResponse* response) override {
    if (!cluster_->HasTable(request->name())) {
      return ToGrpcStatus(NotFoundError(
          "Table does not exist.",
          GCP_ERROR_INFO().WithMetadata("table_name", request->name())));
    }
    if (request->consistency_token() != "some fake token") {
      return ToGrpcStatus(NotFoundError(
          "Unknown consistency token.",
          GCP_ERROR_INFO().WithMetadata("consistency_token",
                                        request->consistency_token())));
    }
    // Emulator is always consistent.
    response->set_consistent(true);
    return grpc::Status::OK;
  }

 private:
  std::shared_ptr<Cluster> cluster_;
};

class DefaultEmulatorServer : public EmulatorServer {
 public:
  DefaultEmulatorServer(std::string const& host, std::uint16_t port)
      : bound_port_(port),
        cluster_(std::make_shared<Cluster>()),
        bt_service_(cluster_),
        table_service_(cluster_) {
    builder_.AddListeningPort(host + ":" + std::to_string(port),
                              grpc::InsecureServerCredentials(), &bound_port_);
    builder_.RegisterService(&bt_service_);
    builder_.RegisterService(&table_service_);
    server_ = builder_.BuildAndStart();
  }
  int bound_port() override { return bound_port_; }
  void Shutdown() override { server_->Shutdown(); }
  void Wait() override { server_->Wait(); }

 private:
  int bound_port_;
  std::shared_ptr<Cluster> cluster_;
  EmulatorService bt_service_;
  EmulatorTableService table_service_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
};

std::unique_ptr<EmulatorServer> CreateDefaultEmulatorServer(
    std::string const& host, std::uint16_t port) {
  return std::unique_ptr<EmulatorServer>(new DefaultEmulatorServer(host, port));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
