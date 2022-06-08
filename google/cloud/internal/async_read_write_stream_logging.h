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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_LOGGING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_LOGGING_H

#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcLogging
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  AsyncStreamingReadWriteRpcLogging(
      std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> child,
      TracingOptions tracing_options, std::string request_id)
      : child_(std::move(child)),
        tracing_options_(std::move(tracing_options)),
        request_id_(std::move(request_id)) {}

  void Cancel() override {
    GCP_LOG(DEBUG) << "Cancel(" << request_id_ << ") <<";
    child_->Cancel();
  }

  future<bool> Start() override {
    auto prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << " <<";
    return child_->Start().then([prefix](future<bool> f) {
      auto r = f.get();
      GCP_LOG(DEBUG) << prefix << " >> " << (r ? "true" : "false");
      return r;
    });
  }

  future<absl::optional<Response>> Read() override {
    auto prefix = std::string(__func__) + "(" + request_id_ + ")";
    auto const& opt = tracing_options_;
    GCP_LOG(DEBUG) << prefix << " <<";
    return child_->Read().then(
        [prefix, opt](future<absl::optional<Response>> f) {
          auto r = f.get();
          if (r.has_value()) {
            GCP_LOG(DEBUG) << prefix << " >> " << DebugString(*r, opt);
          } else {
            GCP_LOG(DEBUG) << prefix << " >> [not-set]";
          }
          return r;
        });
  }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions options) override {
    auto prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << " << "
                   << DebugString(request, tracing_options_);
    return child_->Write(request, std::move(options))
        .then([prefix](future<bool> f) {
          auto r = f.get();
          GCP_LOG(DEBUG) << prefix << " >> " << (r ? "true" : "false");
          return r;
        });
  }

  future<bool> WritesDone() override {
    auto prefix = std::string(__func__) + "(" + request_id_ + ")";
    GCP_LOG(DEBUG) << prefix << " <<";
    return child_->WritesDone().then([prefix](future<bool> f) {
      auto r = f.get();
      GCP_LOG(DEBUG) << prefix << " >> " << (r ? "true" : "false");
      return r;
    });
  }

  future<Status> Finish() override {
    auto prefix = std::string(__func__) + "(" + request_id_ + ")";
    auto const& opt = tracing_options_;
    GCP_LOG(DEBUG) << prefix << " <<";
    return child_->Finish().then([prefix, opt](future<Status> f) {
      auto status = f.get();
      GCP_LOG(DEBUG) << prefix << " >> " << DebugString(status, opt);
      return status;
    });
  }

 private:
  std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> child_;
  TracingOptions tracing_options_;
  std::string request_id_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_LOGGING_H
