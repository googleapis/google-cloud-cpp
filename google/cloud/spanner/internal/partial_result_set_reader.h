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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_READER_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_READER_H_

#include "google/cloud/spanner/result_set.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/optional.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
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
 * This class serves as a bridge between the gRPC `PartialResultSet` streaming
 * reader and the spanner `ResultSet`, which is used to iterate over the rows
 * returned from a read operation.
 */
class PartialResultSetReader : public internal::ResultSetSource {
 public:
  using GrpcReader =
      ::grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>;

  /// Factory method to create a PartialResultSetReader.
  static StatusOr<std::unique_ptr<PartialResultSetReader>> Create(
      std::unique_ptr<GrpcReader> grpc_reader);
  ~PartialResultSetReader() override;

  StatusOr<optional<Value>> NextValue() override;
  optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    return metadata_;
  }
  optional<google::spanner::v1::ResultSetStats> Stats() override {
    return stats_;
  }

 private:
  PartialResultSetReader(std::unique_ptr<GrpcReader> grpc_reader)
      : grpc_reader_(std::move(grpc_reader)) {}

  Status ReadFromStream();

  std::unique_ptr<GrpcReader> grpc_reader_;

  google::protobuf::RepeatedPtrField<google::protobuf::Value> values_;
  optional<google::spanner::v1::ResultSetMetadata> metadata_;
  optional<google::spanner::v1::ResultSetStats> stats_;
  bool finished_ = false;
  int next_value_index_ = 0;
  int next_value_type_index_ = 0;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_READER_H_
