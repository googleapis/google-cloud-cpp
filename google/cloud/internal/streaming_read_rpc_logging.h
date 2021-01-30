// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_LOGGING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_LOGGING_H

#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/status.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/sync_stream.h>
#include <memory>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Logging decorator for StreamingReadRpc.
 */
template <typename ResponseType>
class StreamingReadRpcLogging : public StreamingReadRpc<ResponseType> {
 public:
  StreamingReadRpcLogging(
      std::unique_ptr<StreamingReadRpc<ResponseType>> reader,
      TracingOptions tracing_options, std::string request_id)
      : reader_(std::move(reader)),
        tracing_options_(std::move(tracing_options)),
        request_id_(std::move(request_id)) {}
  ~StreamingReadRpcLogging() override = default;

  void Cancel() override {
    auto const prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << "() >> (void)";
    reader_->Cancel();
    GCP_LOG(DEBUG) << prefix << "() >> (void)";
  }
  absl::variant<Status, ResponseType> Read() override {
    auto const prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << "() >> (void)";
    auto result = reader_->Read();
    GCP_LOG(DEBUG) << prefix << "() >> "
                   << absl::visit(ResultVisitor(tracing_options_), result);
    return result;
  }

 private:
  class ResultVisitor {
   public:
    explicit ResultVisitor(TracingOptions tracing_options)
        : tracing_options_(std::move(tracing_options)) {}

    std::string operator()(Status const& status) {
      std::stringstream output;
      output << status;
      return output.str();
    }
    std::string operator()(ResponseType const& response) {
      return DebugString(response, tracing_options_);
    }

   private:
    TracingOptions tracing_options_;
  };

  std::unique_ptr<StreamingReadRpc<ResponseType>> reader_;
  TracingOptions tracing_options_;
  std::string request_id_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_LOGGING_H
