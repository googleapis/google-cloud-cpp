// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_LOGGING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_LOGGING_H

#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/cloud/log.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Logging decorator for StreamingWriteRpc.
 */
template <typename RequestType, typename ResponseType>
class StreamingWriteRpcLogging
    : public StreamingWriteRpc<RequestType, ResponseType> {
 public:
  StreamingWriteRpcLogging(
      std::unique_ptr<StreamingWriteRpc<RequestType, ResponseType>> stream,
      TracingOptions tracing_options, std::string request_id)
      : stream_(std::move(stream)),
        tracing_options_(std::move(tracing_options)),
        request_id_(std::move(request_id)) {}
  ~StreamingWriteRpcLogging() override = default;

  void Cancel() override {
    auto const prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << "() << (void)";
    stream_->Cancel();
    GCP_LOG(DEBUG) << prefix << "() >> (void)";
  }

  bool Write(RequestType const& request, grpc::WriteOptions options) override {
    auto const prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << "() << "
                   << DebugString(request, tracing_options_);
    auto success = stream_->Write(request, std::move(options));
    GCP_LOG(DEBUG) << prefix << "() >> " << (success ? "true" : "false");
    return success;
  }

  StatusOr<ResponseType> Close() override {
    auto const prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << "() << (void)";
    auto result = stream_->Close();
    if (result) {
      GCP_LOG(DEBUG) << prefix << "() << "
                     << DebugString(*result, tracing_options_);
    } else {
      GCP_LOG(DEBUG) << prefix << "() << "
                     << DebugString(result.status(), tracing_options_);
    }
    return result;
  }

 private:
  std::unique_ptr<StreamingWriteRpc<RequestType, ResponseType>> stream_;
  TracingOptions tracing_options_;
  std::string request_id_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_LOGGING_H
