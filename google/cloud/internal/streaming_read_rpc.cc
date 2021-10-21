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

#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/log.h"
#include <grpc/compression.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {
// AFAICT there is no C++ API to get the name, but the C core API is public and
// documented:
//    https://grpc.github.io/grpc/core/compression_8h.html
std::string ToString(grpc_compression_algorithm algo) {
  char const* name;
  if (grpc_compression_algorithm_name(algo, &name) == 0) {
    return "unknown";
  }
  return name;
}
}  // namespace

StreamingRpcMetadata GetRequestMetadataFromContext(
    grpc::ClientContext const& context) {
  StreamingRpcMetadata metadata{
      // Use invalid header names (starting with ':') to store the
      // grpc::ClientContext metadata.
      {":grpc-context-peer", context.peer()},
      {":grpc-context-compression-algorithm",
       ToString(context.compression_algorithm())},
  };
  auto hint = metadata.end();
  for (auto const& kv : context.GetServerInitialMetadata()) {
    // gRPC metadata is stored in `grpc::string_ref`, a type inspired by
    // `std::string_view`. We need to explicitly convert these to `std::string`.
    // In addition, we use a prefix to distinguish initial vs. trailing headers.
    auto key = ":grpc-initial-" + std::string{kv.first.data(), kv.first.size()};
    auto value = std::string{kv.second.data(), kv.second.size()};
    hint = std::next(
        metadata.emplace_hint(hint, std::move(key), std::move(value)));
  }
  hint = metadata.end();
  for (auto const& kv : context.GetServerTrailingMetadata()) {
    // Same as above, convert `grpc::string_ref` to `std::string`:
    auto key =
        ":grpc-trailing-" + std::string{kv.first.data(), kv.first.size()};
    auto value = std::string{kv.second.data(), kv.second.size()};
    hint = std::next(
        metadata.emplace_hint(hint, std::move(key), std::move(value)));
  }
  return metadata;
}

void StreamingReadRpcReportUnhandledError(Status const& status,
                                          char const* tname) {
  GCP_LOG(WARNING) << "unhandled error for StreamingReadRpcImpl< " << tname
                   << " > - status=" << status;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
