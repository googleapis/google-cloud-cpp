// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/benchmarks/embedded_server.h"
#include "google/cloud/bigtable/benchmarks/random_mutation.h"
#include "google/cloud/bigtable/benchmarks/setup.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <atomic>
#include <iomanip>
#include <sstream>

namespace btproto = google::bigtable::v2;
namespace btadmin = google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
/**
 * Implement the portions of the `google.bigtable.v2.Bigtable` interface
 * necessary for the benchmarks.
 *
 * This is not a Mock (use `google::bigtable::v2::MockBigtableStub` for that,
 * nor is this a Fake implementation (use the Cloud Bigtable Emulator for that),
 * this is an implementation of the interface that returns hardcoded values.
 * It is suitable for the benchmarks, but for nothing else.
 */
class BigtableImpl final : public btproto::Bigtable::Service {
 public:
  BigtableImpl()
      : mutate_row_count_(0), mutate_rows_count_(0), read_rows_count_(0) {
    // Prepare a list of random values to use at run-time.  This is because we
    // want the overhead of this implementation to be as small as possible.
    // Using a single value is an option, but compresses too well and makes the
    // tests a bit unrealistic.
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    values_.resize(1000);
    std::generate(values_.begin(), values_.end(),
                  [&generator]() { return MakeRandomValue(generator); });
  }

  grpc::Status MutateRow(grpc::ServerContext*, btproto::MutateRowRequest const*,
                         btproto::MutateRowResponse*) override {
    ++mutate_row_count_;
    return grpc::Status::OK;
  }

  grpc::Status MutateRows(
      grpc::ServerContext*, btproto::MutateRowsRequest const* request,
      grpc::ServerWriter<btproto::MutateRowsResponse>* writer) override {
    ++mutate_rows_count_;
    btproto::MutateRowsResponse msg;
    for (int index = 0; index != request->entries_size(); ++index) {
      auto& entry = *msg.add_entries();
      entry.set_index(index);
      entry.mutable_status()->set_code(grpc::StatusCode::OK);
    }
    writer->WriteLast(msg, grpc::WriteOptions());
    return grpc::Status::OK;
  }

  grpc::Status ReadRows(
      grpc::ServerContext*, btproto::ReadRowsRequest const* request,
      grpc::ServerWriter<btproto::ReadRowsResponse>* writer) override {
    ++read_rows_count_;
    std::int64_t rows_limit = 10000;
    if (request->rows_limit() != 0) {
      rows_limit = request->rows_limit();
    }

    btproto::ReadRowsResponse msg;
    for (std::int64_t i = 0; i != rows_limit; ++i) {
      std::size_t idx = 0;
      char const* cf = kColumnFamily;
      std::ostringstream os;
      os << "user" << std::setw(12) << std::setfill('0') << i;
      std::string row_key = os.str();
      for (int j = 0; j != kNumFields; ++j) {
        auto& chunk = *msg.add_chunks();
        // This is neither the real format of the keys, nor the keys requested,
        // but it is good enough for a simulation.
        chunk.set_row_key(row_key);
        chunk.set_timestamp_micros(0);
        chunk.mutable_family_name()->set_value(cf);
        chunk.mutable_qualifier()->set_value("field" + std::to_string(j));
        chunk.set_value(values_[idx]);
        chunk.set_value_size(static_cast<std::int32_t>(values_[idx].size()));
        if (++idx >= values_.size()) {
          idx = 0;
        }
        cf = "";
        if (j == kNumFields - 1) {
          chunk.set_value_size(0);
          chunk.set_commit_row(true);
        }
      }
      if (i != request->rows_limit() - 1) {
        writer->Write(msg);
        msg = {};
      }
    }
    writer->WriteLast(msg, grpc::WriteOptions());
    return grpc::Status::OK;
  }

  int mutate_row_count() const { return mutate_row_count_.load(); }
  int mutate_rows_count() const { return mutate_rows_count_.load(); }
  int read_rows_count() const { return read_rows_count_.load(); }

 private:
  std::vector<std::string> values_;
  std::atomic<int> mutate_row_count_;
  std::atomic<int> mutate_rows_count_;
  std::atomic<int> read_rows_count_;
};

/**
 * Implement the `google.bigtable.admin.v2.BigtableTableAdmin` interface for the
 * benchmarks.
 */
class TableAdminImpl final : public btadmin::BigtableTableAdmin::Service {
 public:
  TableAdminImpl() : create_table_count_(0), delete_table_count_(0) {}

  grpc::Status CreateTable(grpc::ServerContext*,
                           btadmin::CreateTableRequest const* request,
                           btadmin::Table* response) override {
    ++create_table_count_;
    response->set_name(request->parent() + "/tables/" + request->table_id());
    return grpc::Status::OK;
  }

  grpc::Status DeleteTable(grpc::ServerContext*,
                           btadmin::DeleteTableRequest const*,
                           ::google::protobuf::Empty*) override {
    ++delete_table_count_;
    return grpc::Status::OK;
  }

  int create_table_count() const { return create_table_count_.load(); }
  int delete_table_count() const { return delete_table_count_.load(); }

 private:
  std::atomic<int> create_table_count_;
  std::atomic<int> delete_table_count_;
};

/// The implementation of EmbeddedServer.
class DefaultEmbeddedServer : public EmbeddedServer {
 public:
  explicit DefaultEmbeddedServer() {
    int port;
    std::string server_address("[::]:0");
    builder_.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                              &port);
    builder_.RegisterService(&bigtable_service_);
    builder_.RegisterService(&admin_service_);
    server_ = builder_.BuildAndStart();
    address_ = "localhost:" + std::to_string(port);
  }

  std::string address() const override { return address_; }
  void Shutdown() override { server_->Shutdown(); }
  void Wait() override { server_->Wait(); }

  int create_table_count() const override {
    return admin_service_.create_table_count();
  }
  int delete_table_count() const override {
    return admin_service_.delete_table_count();
  }
  int mutate_row_count() const override {
    return bigtable_service_.mutate_row_count();
  }
  int mutate_rows_count() const override {
    return bigtable_service_.mutate_rows_count();
  }
  int read_rows_count() const override {
    return bigtable_service_.read_rows_count();
  }

 private:
  BigtableImpl bigtable_service_;
  TableAdminImpl admin_service_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
  std::string address_;
};

std::unique_ptr<EmbeddedServer> CreateEmbeddedServer() {
  return std::unique_ptr<EmbeddedServer>(new DefaultEmbeddedServer);
}

}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
