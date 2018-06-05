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

#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/grpc_error.h"
#include <gtest/gtest.h>

using namespace google::cloud::bigtable::internal;

namespace {
std::string const cmsg("testing with std::string const&");
char const* msg = "testing with char const*";
}  // anonymous namespace

TEST(ThrowDelegateTest, RpcError) {
  grpc::Status status(grpc::StatusCode::UNAVAILABLE, "try-again");
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(RaiseRpcError(status, msg), google::cloud::bigtable::GRpcError);
  EXPECT_THROW(RaiseRpcError(status, cmsg), google::cloud::bigtable::GRpcError);
#else
  EXPECT_DEATH_IF_SUPPORTED(RaiseRpcError(status, msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(RaiseRpcError(status, cmsg), cmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
