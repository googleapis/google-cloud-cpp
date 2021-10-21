// Copyright 2018 Google LLC
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

#include "google/cloud/storage/storage_class.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace storage_class {
namespace {

/// @test Verify the values for the storage class functions.
TEST(StorageClassTest, Values) {
  EXPECT_EQ(std::string("STANDARD"), Standard());
  EXPECT_EQ(std::string("MULTI_REGIONAL"), MultiRegional());
  EXPECT_EQ(std::string("REGIONAL"), Regional());
  EXPECT_EQ(std::string("NEARLINE"), Nearline());
  EXPECT_EQ(std::string("COLDLINE"), Coldline());
  EXPECT_EQ(std::string("DURABLE_REDUCED_AVAILABILITY"),
            DurableReducedAvailability());
  EXPECT_EQ(std::string("ARCHIVE"), Archive());
}

}  // namespace
}  // namespace storage_class
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
