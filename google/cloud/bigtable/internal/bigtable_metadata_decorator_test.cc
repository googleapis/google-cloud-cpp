// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/internal/bigtable_metadata_decorator.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::Pair;

class BigtableMetadataTest : public ::testing::Test {
 protected:
  void SetUp() override { mock_ = std::make_shared<MockBigtableStub>(); }

  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& client_context) {
    return validate_metadata_fixture_.GetMetadata(client_context);
  }

  std::shared_ptr<MockBigtableStub> mock_;

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(BigtableMetadataTest, MutateRowAppProfileIdEmpty) {
  EXPECT_CALL(*mock_, MutateRow)
      .WillOnce(::testing::Return(google::bigtable::v2::MutateRowResponse{}));

  BigtableMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest request;
  request.set_table_name("projects/p/instances/i/tables/t");

  auto status = stub.MutateRow(context, Options{}, request);
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Contains(Pair("x-goog-request-params",
                                      "app_profile_id=&table_name=projects%2Fp%"
                                      "2Finstances%2Fi%2Ftables%2Ft")));
}

TEST_F(BigtableMetadataTest, MutateRowAppProfileIdNotEmpty) {
  EXPECT_CALL(*mock_, MutateRow)
      .WillOnce(::testing::Return(google::bigtable::v2::MutateRowResponse{}));

  BigtableMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest request;
  request.set_table_name("projects/p/instances/i/tables/t");
  request.set_app_profile_id("my-profile");

  auto status = stub.MutateRow(context, Options{}, request);
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata,
              Contains(Pair("x-goog-request-params",
                            "app_profile_id=my-profile&table_name=projects%2Fp%"
                            "2Finstances%2Fi%2Ftables%2Ft")));
}

TEST_F(BigtableMetadataTest, MutateRowIsContextMDValidWithEmptyAppProfileId) {
  EXPECT_CALL(*mock_, MutateRow)
      .WillOnce([](grpc::ClientContext& context, Options const&,
                   google::bigtable::v2::MutateRowRequest const& request) {
        ValidateMetadataFixture fixture;
        fixture.IsContextMDValid(
            context, "google.bigtable.v2.Bigtable.MutateRow", request,
            google::cloud::internal::GeneratedLibClientHeader());
        return google::bigtable::v2::MutateRowResponse{};
      });

  BigtableMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::bigtable::v2::MutateRowRequest request;
  request.set_table_name("projects/p/instances/i/tables/t");

  auto status = stub.MutateRow(context, Options{}, request);
  EXPECT_STATUS_OK(status);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
