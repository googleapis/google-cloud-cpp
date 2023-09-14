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

#include "google/cloud/testing_util/validate_propagator.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {
namespace {
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Pair;
}  // namespace

void ValidatePropagator(grpc::ClientContext& context) {
  ValidateMetadataFixture fixture;
  auto md = fixture.GetMetadata(context);
  EXPECT_THAT(md, IsSupersetOf({Pair("x-cloud-trace-context", _),
                                Pair("traceparent", _)}));
}

void ValidateNoPropagator(grpc::ClientContext& context) {
  ValidateMetadataFixture fixture;
  auto md = fixture.GetMetadata(context);
  EXPECT_THAT(md, AllOf(Not(Contains(Pair("x-cloud-trace-context", _))),
                        Not(Contains(Pair("traceparent", _)))));
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
