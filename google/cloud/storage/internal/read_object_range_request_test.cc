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

#include "google/cloud/storage/internal/read_object_range_request.h"
#include <gtest/gtest.h>

using namespace storage::internal;

TEST(ReadObjectRangeRequest, Simple) {
  ReadObjectRangeRequest request("my-bucket", "my-object", 0, 1024);

  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());
  EXPECT_EQ(0, request.begin());
  EXPECT_EQ(1024, request.end());

  request.set_parameter(storage::UserProject("my-project"));
  request.set_multiple_parameters(storage::IfGenerationMatch(7),
                                  storage::UserProject("my-project"));
}
