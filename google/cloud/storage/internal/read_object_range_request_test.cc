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

namespace {
HttpResponse CreateRangeRequestResponse(
    char const* content_range_header_value) {
  HttpResponse response;
  response.headers.emplace(std::string("content-range"),
                           std::string(content_range_header_value));
  response.payload = "some payload";
  return response;
}
}  // namespace

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

TEST(ReadObjectRangeResponse, Parse) {
  auto actual = ReadObjectRangeResponse::FromHttpResponse(
      CreateRangeRequestResponse("bytes 100-200/20000"));
  EXPECT_EQ(100, actual.first_byte);
  EXPECT_EQ(200, actual.last_byte);
  EXPECT_EQ(20000, actual.object_size);
  EXPECT_EQ("some payload", actual.contents);
}

TEST(ReadObjectRangeResponse, ParseStar) {
  auto actual = ReadObjectRangeResponse::FromHttpResponse(
      CreateRangeRequestResponse("bytes */20000"));
  EXPECT_EQ(0, actual.first_byte);
  EXPECT_EQ(0, actual.last_byte);
  EXPECT_EQ(20000, actual.object_size);
}

TEST(ReadObjectRangeResponse, ParseErrors) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bits 100-200/20000")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("100-200/20000")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes ")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes */")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes 100-200/")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes 100-/20000")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes -200/20000")),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      ReadObjectRangeResponse::FromHttpResponse(
          CreateRangeRequestResponse("bits 100-200/20000")),
      "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("100-200/20000")),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes ")),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes */")),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes 100-200/")),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes 100-/20000")),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes -200/20000")),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}