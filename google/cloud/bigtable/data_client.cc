// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include "google/cloud/bigtable/internal/logging_data_client.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace btproto = ::google::bigtable::v2;

std::unique_ptr<
    ::grpc::ClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>
// NOLINTNEXTLINE(performance-unnecessary-value-param)
DataClient::PrepareAsyncSampleRowKeys(grpc::ClientContext*,
                                      btproto::SampleRowKeysRequest const&,
                                      grpc::CompletionQueue*) {
  return nullptr;
}

namespace {

/**
 * Implement a simple DataClient.
 *
 * This implementation does not support multiple threads, or refresh
 * authorization tokens.  In other words, it is extremely bare bones.
 */
class DefaultDataClient : public DataClient {
 public:
  DefaultDataClient(std::string project, std::string instance,
                    Options options = {})
      : project_(std::move(project)),
        instance_(std::move(instance)),
        authority_(options.get<AuthorityOption>()),
        user_project_(
            options.has<UserProjectOption>()
                ? absl::make_optional(options.get<UserProjectOption>())
                : absl::nullopt),
        impl_(std::move(options)) {}

  std::string const& project_id() const override { return project_; };
  std::string const& instance_id() const override { return instance_; };

  std::shared_ptr<grpc::Channel> Channel() override { return impl_.Channel(); }
  void reset() override { impl_.reset(); }

  grpc::Status MutateRow(grpc::ClientContext* context,
                         btproto::MutateRowRequest const& request,
                         btproto::MutateRowResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->MutateRow(context, request, response);
  }

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<btproto::MutateRowResponse>>
  AsyncMutateRow(grpc::ClientContext* context,
                 btproto::MutateRowRequest const& request,
                 grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncMutateRow(context, request, cq);
  }

  grpc::Status CheckAndMutateRow(
      grpc::ClientContext* context,
      btproto::CheckAndMutateRowRequest const& request,
      btproto::CheckAndMutateRowResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->CheckAndMutateRow(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      btproto::CheckAndMutateRowResponse>>
  AsyncCheckAndMutateRow(grpc::ClientContext* context,
                         btproto::CheckAndMutateRowRequest const& request,
                         grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncCheckAndMutateRow(context, request, cq);
  }

  grpc::Status ReadModifyWriteRow(
      grpc::ClientContext* context,
      btproto::ReadModifyWriteRowRequest const& request,
      btproto::ReadModifyWriteRowResponse* response) override {
    ApplyOptions(context);
    return impl_.Stub()->ReadModifyWriteRow(context, request, response);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      btproto::ReadModifyWriteRowResponse>>
  AsyncReadModifyWriteRow(grpc::ClientContext* context,
                          btproto::ReadModifyWriteRowRequest const& request,
                          grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncReadModifyWriteRow(context, request, cq);
  }

  std::unique_ptr<grpc::ClientReaderInterface<btproto::ReadRowsResponse>>
  ReadRows(grpc::ClientContext* context,
           btproto::ReadRowsRequest const& request) override {
    ApplyOptions(context);
    return impl_.Stub()->ReadRows(context, request);
  }

  std::unique_ptr<grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse>>
  AsyncReadRows(grpc::ClientContext* context,
                btproto::ReadRowsRequest const& request,
                grpc::CompletionQueue* cq, void* tag) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncReadRows(context, request, cq, tag);
  }

  std::unique_ptr<::grpc::ClientAsyncReaderInterface<btproto::ReadRowsResponse>>
  PrepareAsyncReadRows(grpc::ClientContext* context,
                       btproto::ReadRowsRequest const& request,
                       grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->PrepareAsyncReadRows(context, request, cq);
  }

  std::unique_ptr<grpc::ClientReaderInterface<btproto::SampleRowKeysResponse>>
  SampleRowKeys(grpc::ClientContext* context,
                btproto::SampleRowKeysRequest const& request) override {
    ApplyOptions(context);
    return impl_.Stub()->SampleRowKeys(context, request);
  }

  std::unique_ptr<
      ::grpc::ClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>
  AsyncSampleRowKeys(grpc::ClientContext* context,
                     btproto::SampleRowKeysRequest const& request,
                     grpc::CompletionQueue* cq, void* tag) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncSampleRowKeys(context, request, cq, tag);
  }

  std::unique_ptr<
      ::grpc::ClientAsyncReaderInterface<btproto::SampleRowKeysResponse>>
  PrepareAsyncSampleRowKeys(grpc::ClientContext* context,
                            btproto::SampleRowKeysRequest const& request,
                            grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->PrepareAsyncSampleRowKeys(context, request, cq);
  }

  std::unique_ptr<grpc::ClientReaderInterface<btproto::MutateRowsResponse>>
  MutateRows(grpc::ClientContext* context,
             btproto::MutateRowsRequest const& request) override {
    ApplyOptions(context);
    return impl_.Stub()->MutateRows(context, request);
  }

  std::unique_ptr<
      ::grpc::ClientAsyncReaderInterface<btproto::MutateRowsResponse>>
  AsyncMutateRows(grpc::ClientContext* context,
                  btproto::MutateRowsRequest const& request,
                  grpc::CompletionQueue* cq, void* tag) override {
    ApplyOptions(context);
    return impl_.Stub()->AsyncMutateRows(context, request, cq, tag);
  }

  std::unique_ptr<
      ::grpc::ClientAsyncReaderInterface<btproto::MutateRowsResponse>>
  PrepareAsyncMutateRows(grpc::ClientContext* context,
                         btproto::MutateRowsRequest const& request,
                         grpc::CompletionQueue* cq) override {
    ApplyOptions(context);
    return impl_.Stub()->PrepareAsyncMutateRows(context, request, cq);
  }

 private:
  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return impl_.BackgroundThreadsFactory();
  }

  std::shared_ptr<grpc::Channel> ChannelImpl() override {
    return impl_.Channel();
  }
  void resetImpl() override { impl_.reset(); }

  void ApplyOptions(grpc::ClientContext* context) {
    if (!authority_.empty()) context->set_authority(authority_);
    if (user_project_) {
      context->AddMetadata("x-goog-user-project", *user_project_);
    }
  }

  std::string project_;
  std::string instance_;
  std::string authority_;
  absl::optional<std::string> user_project_;
  internal::CommonClient<btproto::Bigtable> impl_;
};

}  // namespace

std::shared_ptr<DataClient> MakeDataClient(std::string project_id,
                                           std::string instance_id,
                                           Options options) {
  options = internal::DefaultDataOptions(std::move(options));
  bool tracing_enabled = google::cloud::internal::Contains(
      options.get<TracingComponentsOption>(), "rpc");
  auto tracing_options = options.get<GrpcTracingOptionsOption>();

  std::shared_ptr<DataClient> client = std::make_shared<DefaultDataClient>(
      std::move(project_id), std::move(instance_id), std::move(options));
  if (tracing_enabled) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    client = std::make_shared<internal::LoggingDataClient>(
        std::move(client), std::move(tracing_options));
  }
  return client;
}

std::shared_ptr<DataClient> CreateDefaultDataClient(std::string project_id,
                                                    std::string instance_id,
                                                    ClientOptions options) {
  return MakeDataClient(std::move(project_id), std::move(instance_id),
                        internal::MakeOptions(std::move(options)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
