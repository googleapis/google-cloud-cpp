// Copyright 2018 Google Inc.
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

#include "bigtable/client/grpc_error.h"
#include <gmock/gmock.h>

TEST(GRpcErrorTest, Simple) {
  bigtable::GRpcError cancelled("Test()", grpc::Status::CANCELLED);
  EXPECT_EQ(grpc::Status::CANCELLED.error_code(), cancelled.error_code());
  EXPECT_EQ(grpc::Status::CANCELLED.error_message(), cancelled.error_message());
  EXPECT_EQ(grpc::Status::CANCELLED.error_details(), cancelled.error_details());

  bigtable::GRpcError test("Test()", grpc::Status(grpc::StatusCode::UNAVAILABLE,
                                                  "try-again", "too-busy"));
  EXPECT_EQ(grpc::StatusCode::UNAVAILABLE, test.error_code());
  EXPECT_EQ("try-again", test.error_message());
  EXPECT_EQ("too-busy", test.error_details());

  std::string what(test.what());
  EXPECT_NE(std::string::npos, what.find("Test()"));
  EXPECT_NE(std::string::npos, what.find("try-again"));
  EXPECT_NE(std::string::npos, what.find("too-busy"));
  EXPECT_NE(std::string::npos, what.find("UNAVAILABLE"));
}

TEST(GRpcErrorTest, KnownCode_UNAUTHENTICATED) {
  bigtable::GRpcError ex(
      "T()", grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "", ""));
  EXPECT_EQ(grpc::StatusCode::UNAUTHENTICATED, ex.error_code());
  EXPECT_NE(std::string::npos, std::string(ex.what()).find("UNAUTHENTICATED"));
}

TEST(GRpcErrorTest, KnownCode_DATA_LOSS) {
  bigtable::GRpcError ex("T()",
                         grpc::Status(grpc::StatusCode::DATA_LOSS, "", ""));
  EXPECT_EQ(grpc::StatusCode::DATA_LOSS, ex.error_code());
  EXPECT_NE(std::string::npos, std::string(ex.what()).find("DATA_LOSS"));
}

TEST(GRpcErrorTest, KnownCode_NOT_FOUND) {
  bigtable::GRpcError ex("T()",
                         grpc::Status(grpc::StatusCode::NOT_FOUND, "", ""));
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, ex.error_code());
  EXPECT_NE(std::string::npos, std::string(ex.what()).find("NOT_FOUND"));
}
