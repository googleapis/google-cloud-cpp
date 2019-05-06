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

#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/internal/common_client.h"

namespace btproto = google::bigtable::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Implement a simple DataClient.
 *
 * This implementation does not support multiple threads, or refresh
 * authorization tokens.  In other words, it is extremely bare bones.
 */
class DefaultDataClient : public DataClient {
 private:
  // Introduce an early `private:` section because this type is used to define
  // the public interface, it should not be part of the public interface.
  struct DataTraits {
    static std::string const& Endpoint(bigtable::ClientOptions& options) {
      return options.data_endpoint();
    }
  };

  using Impl = bigtable::internal::CommonClient<DataTraits, btproto::Bigtable>;

 public:
  DefaultDataClient(std::string project, std::string instance,
                    ClientOptions options)
      : project_(std::move(project)),
        instance_(std::move(instance)),
        impl_(std::move(options)) {}

  DefaultDataClient(std::string project, std::string instance)
      : DefaultDataClient(std::move(project), std::move(instance),
                          ClientOptions()) {}

  std::string const& project_id() const override;
  std::string const& instance_id() const override;

  std::shared_ptr<grpc::Channel> Channel() override { return impl_.Channel(); }
  void reset() override { impl_.reset(); }

  grpc::Status MutateRow(grpc::ClientContext* context,
                         btproto::MutateRowRequest const& request,
                         btproto::MutateRowResponse* response) override {
    return impl_.Stub()->MutateRow(context, request, response);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>
  AsyncMutateRow(grpc::ClientContext* context,
                 btproto::MutateRowRequest const& request,
                 grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncMutateRow(context, request, cq);
  }

  grpc::Status CheckAndMutateRow(
      grpc::ClientContext* context,
      btproto::CheckAndMutateRowRequest const& request,
      btproto::CheckAndMutateRowResponse* response) override {
    return impl_.Stub()->CheckAndMutateRow(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::v2::CheckAndMutateRowResponse>>
  AsyncCheckAndMutateRow(
      grpc::ClientContext* context,
      const google::bigtable::v2::CheckAndMutateRowRequest& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncCheckAndMutateRow(context, request, cq);
  }

  grpc::Status ReadModifyWriteRow(
      grpc::ClientContext* context,
      btproto::ReadModifyWriteRowRequest const& request,
      btproto::ReadModifyWriteRowResponse* response) override {
    return impl_.Stub()->ReadModifyWriteRow(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::v2::ReadModifyWriteRowResponse>>
  AsyncReadModifyWriteRow(
      grpc::ClientContext* context,
      google::bigtable::v2::ReadModifyWriteRowRequest const& request,
      grpc::CompletionQueue* cq) override {
    return impl_.Stub()->AsyncReadModifyWriteRow(context, request, cq);
  }

  std::unique_ptr<grpc::ClientReaderInterface<btproto::ReadRowsResponse>>
  ReadRows(grpc::ClientContext* context,
           btproto::ReadRowsRequest const& request) override {
    return impl_.Stub()->ReadRows(context, request);
  }

  std::unique_ptr<grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse>>
  AsyncReadRows(grpc::ClientContext* context,
                const google::bigtable::v2::ReadRowsRequest& request,
                grpc::CompletionQueue* cq, void* tag) override {
    return impl_.Stub()->AsyncReadRows(context, request, cq, tag);
  }

  std::unique_ptr<grpc::ClientReaderInterface<btproto::SampleRowKeysResponse>>
  SampleRowKeys(grpc::ClientContext* context,
                btproto::SampleRowKeysRequest const& request) override {
    return impl_.Stub()->SampleRowKeys(context, request);
  }
  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::SampleRowKeysResponse>>
  AsyncSampleRowKeys(
      ::grpc::ClientContext* context,
      const ::google::bigtable::v2::SampleRowKeysRequest& request,
      ::grpc::CompletionQueue* cq, void* tag) override {
    return impl_.Stub()->AsyncSampleRowKeys(context, request, cq, tag);
  }

  std::unique_ptr<grpc::ClientReaderInterface<btproto::MutateRowsResponse>>
  MutateRows(grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request) override {
    return impl_.Stub()->MutateRows(context, request);
  }
  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::MutateRowsResponse>>
  AsyncMutateRows(::grpc::ClientContext* context,
                  const ::google::bigtable::v2::MutateRowsRequest& request,
                  ::grpc::CompletionQueue* cq, void* tag) override {
    return impl_.Stub()->AsyncMutateRows(context, request, cq, tag);
  }
  std::unique_ptr<::grpc::ClientAsyncReaderInterface<
      ::google::bigtable::v2::MutateRowsResponse>>
  PrepareAsyncMutateRows(
      ::grpc::ClientContext* context,
      const ::google::bigtable::v2::MutateRowsRequest& request,
      ::grpc::CompletionQueue* cq) override {
    return impl_.Stub()->PrepareAsyncMutateRows(context, request, cq);
  }

 private:
  std::string project_;
  std::string instance_;
  Impl impl_;
};

std::string const& DefaultDataClient::project_id() const { return project_; }

std::string const& DefaultDataClient::instance_id() const { return instance_; }
}  // namespace internal

std::shared_ptr<DataClient> CreateDefaultDataClient(std::string project_id,
                                                    std::string instance_id,
                                                    ClientOptions options) {
  return std::make_shared<internal::DefaultDataClient>(
      std::move(project_id), std::move(instance_id), std::move(options));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
