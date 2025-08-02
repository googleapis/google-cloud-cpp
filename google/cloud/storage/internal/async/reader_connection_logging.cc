// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/reader_connection_logging.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage_experimental::AsyncReaderConnection;
using ::google::cloud::storage_experimental::ReadPayload;

class ReaderConnectionLogging : public AsyncReaderConnection {
 public:
  explicit ReaderConnectionLogging(std::unique_ptr<AsyncReaderConnection> child)
      : child_(std::move(child)) {}

  void Cancel() override {
    GCP_LOG(DEBUG) << "ReaderConnectionLogging::Cancel()";
    child_->Cancel();
  }

  future<ReadResponse> Read() override {
    GCP_LOG(DEBUG) << "ReaderConnectionLogging::Read() <<";
    return child_->Read().then([](auto f) {
      auto response = f.get();
      if (absl::holds_alternative<Status>(response)) {
        GCP_LOG(DEBUG) << "ReaderConnectionLogging::Read() >> status="
                       << absl::get<Status>(response).message();
      } else {
        auto const& payload = absl::get<ReadPayload>(response);
        GCP_LOG(DEBUG) << "ReaderConnectionLogging::Read() >>"
                       << " payload.size=" << payload.size()
                       << ", offset=" << payload.offset();
      }
      return response;
    });
  }

  RpcMetadata GetRequestMetadata() override {
    return child_->GetRequestMetadata();
  }

 private:
  std::unique_ptr<AsyncReaderConnection> child_;
};

}  // namespace

std::unique_ptr<storage_experimental::AsyncReaderConnection>
MakeLoggingReaderConnection(
    Options const& options,
    std::unique_ptr<storage_experimental::AsyncReaderConnection> impl) {
  auto const& components = options.get<LoggingComponentsOption>();
  if (std::find(components.begin(), components.end(), "rpc") ==
      components.end()) {
    return impl;
  }
  return std::make_unique<ReaderConnectionLogging>(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
