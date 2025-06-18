// Copyright 2023 Google LLC
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

#include "generator/testing/descriptor_pool_fixture.h"
#include "generator/testing/error_collectors.h"
#include "google/api/annotations.pb.h"
#include "google/api/client.pb.h"
#include "google/longrunning/operations.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/rpc/status.pb.h"

namespace google {
namespace cloud {
namespace generator_testing {

auto constexpr kEmptyFile = R"""(syntax = "proto3";)""";

auto constexpr kCommonFileContents = R"""(
// We need to test that our generator handles references to different entities.
// This simulated .proto file provides their definition.

syntax = "proto3";
package test.v1;

// A request type for the methods
message Request {}
// A response type for the methods
message Response {}
// A metadata type for some LROs
message Metadata {}
)""";

DescriptorPoolFixture::DescriptorPoolFixture()
    : descriptor_error_collector_(
          std::make_unique<generator_testing::ErrorCollector>()),
      multifile_error_collector_(
          std::make_unique<generator_testing::MultiFileErrorCollector>()),
      source_tree_db_(&source_tree_),
      merged_db_(&simple_db_, &source_tree_db_),
      pool_(&merged_db_, descriptor_error_collector_.get()) {
  source_tree_db_.RecordErrorsTo(multifile_error_collector_.get());
  source_tree_.Insert("test/v1/common.proto", kCommonFileContents);
  source_tree_.Insert("google/api/annotations.proto", kEmptyFile);
  source_tree_.Insert("google/api/client.proto", kEmptyFile);

  // We need google.longrunning.* to be available. This also imports the
  // google.protobuf.*Descriptor protos.
  for (auto const* descriptor : {
           google::protobuf::FileDescriptorProto::descriptor(),
           google::rpc::Status::descriptor(),
           google::protobuf::Any::descriptor(),
           google::protobuf::Duration::descriptor(),
           google::protobuf::Empty::descriptor(),
           google::longrunning::Operation::descriptor(),
       }) {
    google::protobuf::FileDescriptorProto proto;
    descriptor->file()->CopyTo(&proto);
    simple_db_.Add(proto);
  }
}

google::protobuf::FileDescriptor const* DescriptorPoolFixture::FindFile(
    std::string const& name) const {
  return pool_.FindFileByName(name);
}

bool DescriptorPoolFixture::AddProtoFile(std::string const& name,
                                         std::string contents) {
  source_tree_.Insert(name, std::move(contents));
  return pool_.FindFileByName(name) != nullptr;
}

}  // namespace generator_testing
}  // namespace cloud
}  // namespace google
