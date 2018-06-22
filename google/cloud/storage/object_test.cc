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

#include "google/cloud/storage/object.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

using namespace google::cloud::storage;
using namespace ::testing;
using google::cloud::storage::testing::MockClient;

TEST(ObjectTest, CopyConstructor) {
  auto mock = std::make_shared<MockClient>();
  Object object(mock, "my-bucket", "my-object");
  EXPECT_EQ("my-bucket", object.bucket_name());
  EXPECT_EQ("my-object", object.object_name());

  Object copy(object);
  EXPECT_EQ("my-bucket", copy.bucket_name());
  EXPECT_EQ("my-object", copy.object_name());
}

TEST(ObjectTest, CopyAssignment) {
  auto mock = std::make_shared<MockClient>();
  Object object(mock, "my-bucket", "my-object");
  EXPECT_EQ("my-bucket", object.bucket_name());
  EXPECT_EQ("my-object", object.object_name());

  Object copy(mock, "foo", "bar");
  copy = object;
  EXPECT_EQ("my-bucket", copy.bucket_name());
  EXPECT_EQ("my-object", copy.object_name());
}
