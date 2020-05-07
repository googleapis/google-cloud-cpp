// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_READER_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status.h"
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * Wrap grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>.
 *
 * This defines an interface to handle a streaming RPC returning a sequence of
 * `google::spanner::v1::PartialResultSet`. Its main purpose is to simplify
 * memory management, as each streaming RPC requires two separate
 * `std::unique_ptr<>`. As a side-effect, it is easier to mock this class, as
 * it has a narrower interface.
 */
class PartialResultSetReader {
 public:
  virtual ~PartialResultSetReader() = default;
  virtual void TryCancel() = 0;
  virtual optional<google::spanner::v1::PartialResultSet> Read() = 0;
  virtual Status Finish() = 0;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_READER_H
