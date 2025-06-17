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

#include "generator/internal/pagination.h"
#include "generator/testing/descriptor_pool_fixture.h"
#include "generator/testing/error_collectors.h"
#include "generator/testing/fake_source_tree.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/protobuf/descriptor.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FileDescriptorProto;
using ::testing::Eq;
using ::testing::ValuesIn;

TEST(PaginationTest, PaginationAIP4233Success) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_field"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
  auto result =
      DeterminePagination(*service_file_descriptor->service(0)->method(0));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->range_output_field_name, "repeated_field");
  EXPECT_EQ(result->range_output_type->full_name(), "google.protobuf.Bar");
}

TEST(PaginationTest, PaginationAIP4233NoPageSize) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Input" }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationAIP4233NoPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
    }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoPageToken"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationAIP4233NoNextPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type { name: "Output" }
    service {
      name: "Service"
      method {
        name: "NoNextPageToken"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationAIP4233NoRepeatedMessageField) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_field"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_INT32
      }
    }
    service {
      name: "Service"
      method {
        name: "NoRepeatedMessage"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PredicateUtilsDeathTest, PaginationAIP4233RepeatedMessageOrderMismatch) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type { name: "Foo" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_message_field_first"
        number: 3
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Foo"
      }
      field {
        name: "repeated_message_field_second"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "RepeatedOrderMismatch"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_DEATH_IF_SUPPORTED(
      IsPaginated(*service_file_descriptor->service(0)->method(0)),
      "Repeated field in paginated response must be first");
}

TEST(PaginationTest, PaginationAIP4233ExactlyOneRepatedStringResponse) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_size" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "repeated_field"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_STRING
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
  auto result =
      DeterminePagination(*service_file_descriptor->service(0)->method(0));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->range_output_field_name, "repeated_field");
  EXPECT_EQ(result->range_output_type, nullptr);
}

TEST(PaginationTest, PaginationRestSuccess) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
  auto result =
      DeterminePagination(*service_file_descriptor->service(0)->method(0));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->range_output_field_name, "items");
  EXPECT_EQ(result->range_output_type->full_name(), "google.protobuf.Bar");
}

TEST(PaginationTest, PaginationRestNoMaxResults) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationRestMaxResultsWrongType) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_INT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationRestNoPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationRestNoNextPageToken) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field {
        name: "items"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationRestNoItems) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "bars"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

TEST(PaginationTest, PaginationRestItemsNotRepeated) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Bar" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items"
        number: 2
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Bar"
      }
    }
    service {
      name: "Service"
      method {
        name: "NoPageSize"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_FALSE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
}

auto constexpr kBigQueryText = R"pb(
  name: "google/bigquery/bq.proto"
  package: "google.cloud.bigquery.v2"
  message_type { name: "Model" }
  message_type { name: "ListFormatTable" }
  message_type { name: "ListFormatJob" }
  message_type { name: "ListFormatDataset" }
)pb";

auto constexpr kProtobufText = R"pb(
  name: "google/protobuf/pb.proto"
  package: "google.protobuf"
  message_type { name: "Int32Value" }
  message_type { name: "UInt32Value" }
  message_type { name: "Struct" }
)pb";

struct BigQueryTestParams {
  std::string const max_results_field_type_message_name;
  std::string const items_field_name;
  std::string const items_field_type_message_name;
};

class BigQueryTestFixture
    : public ::testing::TestWithParam<BigQueryTestParams> {
 public:
  void AddDependenciesToDatabase() {
    FileDescriptorProto proto_file;
    google::protobuf::TextFormat::ParseFromString(kProtobufText, &proto_file);
    simple_db_.Add(proto_file);
    FileDescriptorProto bq_file;
    google::protobuf::TextFormat::ParseFromString(kBigQueryText, &bq_file);
    simple_db_.Add(bq_file);
  }

 protected:
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  generator_testing::ErrorCollector collector_;
};

TEST_P(BigQueryTestFixture, DetermineBigQueryPagination) {
  BigQueryTestParams params = GetParam();
  AddDependenciesToDatabase();
  FileDescriptorProto service_file;
  /// @cond
  std::string service_text = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.service"
    dependency: "google/protobuf/pb.proto"
    dependency: "google/bigquery/bq.proto"
    message_type {
      name: "Input"
      field {
        name: "max_results"
        number: 1
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "max_results_field_type_message_name_placeholder"
      }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "items_field_name_placeholder"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "items_field_type_message_name_placeholder"
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.service.Input"
        output_type: "google.service.Output"
      }
    }
  )pb";
  /// @endcond

  // Replace the template text with parameters.
  service_text = absl::StrReplaceAll(
      service_text, {{"max_results_field_type_message_name_placeholder",
                      params.max_results_field_type_message_name},
                     {"items_field_name_placeholder", params.items_field_name},
                     {"items_field_type_message_name_placeholder",
                      params.items_field_type_message_name}});
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(service_text,
                                                            &service_file));
  simple_db_.Add(service_file);
  DescriptorPool pool(&simple_db_, &collector_);
  auto const* service_file_descriptor =
      pool.FindFileByName("google/foo/v1/service.proto");

  EXPECT_TRUE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
  auto result =
      DeterminePagination(*service_file_descriptor->service(0)->method(0));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->range_output_field_name, params.items_field_name);
  EXPECT_EQ(result->range_output_type->full_name(),
            params.items_field_type_message_name);
}

INSTANTIATE_TEST_SUITE_P(BigQueryTests, BigQueryTestFixture,
                         ValuesIn<BigQueryTestParams>(
                             {{"google.protobuf.Int32Value", "jobs",
                               "google.cloud.bigquery.v2.ListFormatJob"},
                              {"google.protobuf.UInt32Value", "rows",
                               "google.protobuf.Struct"},
                              {"google.protobuf.UInt32Value", "tables",
                               "google.cloud.bigquery.v2.ListFormatTable"},
                              {"google.protobuf.UInt32Value", "datasets",
                               "google.cloud.bigquery.v2.ListFormatDataset"},
                              {"google.protobuf.UInt32Value", "models",
                               "google.cloud.bigquery.v2.Model"}}));

TEST(PaginationTest, PaginationBigQuerySpecialCaseSuccess) {
  FileDescriptorProto service_file;
  /// @cond
  auto constexpr kServiceText = R"pb(
    name: "google/foo/v1/service.proto"
    package: "google.protobuf"
    message_type { name: "Struct" }
    message_type {
      name: "Input"
      field { name: "max_results" number: 1 type: TYPE_UINT32 }
      field { name: "page_token" number: 2 type: TYPE_STRING }
    }
    message_type {
      name: "Output"
      field { name: "next_page_token" number: 1 type: TYPE_STRING }
      field {
        name: "rows"
        number: 2
        label: LABEL_REPEATED
        type: TYPE_MESSAGE
        type_name: "google.protobuf.Struct"
      }
    }
    service {
      name: "Service"
      method {
        name: "Paginated"
        input_type: "google.protobuf.Input"
        output_type: "google.protobuf.Output"
      }
    }
  )pb";
  /// @endcond
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kServiceText,
                                                            &service_file));
  DescriptorPool pool;
  FileDescriptor const* service_file_descriptor = pool.BuildFile(service_file);
  EXPECT_TRUE(IsPaginated(*service_file_descriptor->service(0)->method(0)));
  auto result =
      DeterminePagination(*service_file_descriptor->service(0)->method(0));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->range_output_field_name, "rows");
  EXPECT_EQ(result->range_output_type->full_name(), "google.protobuf.Struct");
}

auto constexpr kAnnotationsProto = R"""(
    syntax = "proto3";
    package google.api;
    import "google/api/http.proto";
    import "google/protobuf/descriptor.proto";
    extend google.protobuf.MethodOptions {
      // See `HttpRule`.
      HttpRule http = 72295728;
    };
)""";

auto constexpr kHttpProto = R"""(
    syntax = "proto3";
    package google.api;
    option cc_enable_arenas = true;
    message Http {
      repeated HttpRule rules = 1;
      bool fully_decode_reserved_expansion = 2;
    }
    message HttpRule {
      string selector = 1;
      oneof pattern {
        string get = 2;
        string put = 3;
        string post = 4;
        string delete = 5;
        string patch = 6;
        CustomHttpPattern custom = 8;
      }
      string body = 7;
      string response_body = 12;
      repeated HttpRule additional_bindings = 11;
    }
    message CustomHttpPattern {
      string kind = 1;
      string path = 2;
    };
)""";

auto constexpr kServiceProto = R"""(
syntax = "proto3";
package test;
import "google/api/annotations.proto";
import "google/api/http.proto";
// Request message for AggregatedListDisks.
message AggregatedListFoosRequest {
  optional uint32 max_results = 1;
  optional string page_token = 2;
}

message Foo {
  optional string name = 1;
}

message FooAggregatedList {
  map<string, Foo> items = 1;
  optional string next_page_token = 2;
}
// Leading comments about service Service0.
service Service0 {
  // Leading comments about rpc Method0$.
  rpc Method0(AggregatedListFoosRequest) returns (FooAggregatedList) {
    option (google.api.http) = {
       patch: "/v1/{parent=projects/*/instances/*}/databases"
       body: "*"
    };
  }
};
)""";

class PaginationDescriptorTest : public ::testing::Test {
 public:
  PaginationDescriptorTest()
      : source_tree_(std::map<std::string, std::string>{
            {std::string("google/api/http.proto"), kHttpProto},
            {std::string("google/api/annotations.proto"), kAnnotationsProto},
            {std::string("google/foo/v1/service.proto"), kServiceProto}}),
        source_tree_db_(&source_tree_),
        merged_db_(&simple_db_, &source_tree_db_),
        pool_(&merged_db_, &collector_) {
    // we need descriptor.proto to be accessible by the pool
    // since our test file imports it
    FileDescriptorProto::descriptor()->file()->CopyTo(&file_proto_);
    simple_db_.Add(file_proto_);
  }

 private:
  FileDescriptorProto file_proto_;
  generator_testing::ErrorCollector collector_;
  generator_testing::FakeSourceTree source_tree_;
  google::protobuf::SimpleDescriptorDatabase simple_db_;
  google::protobuf::compiler::SourceTreeDescriptorDatabase source_tree_db_;
  google::protobuf::MergedDescriptorDatabase merged_db_;

 protected:
  DescriptorPool pool_;
};

TEST_F(PaginationDescriptorTest, MapPagination) {
  auto const* file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  auto pagination_info =
      DeterminePagination(*file_descriptor->service(0)->method(0));
  ASSERT_TRUE(pagination_info.has_value());
  EXPECT_THAT(pagination_info->range_output_field_name, Eq("items"));
  EXPECT_THAT(pagination_info->range_output_type->name(), Eq("Foo"));
  ASSERT_TRUE(pagination_info->range_output_map_key_type.has_value());
  EXPECT_THAT((*pagination_info->range_output_map_key_type)->cpp_type_name(),
              Eq(std::string("string")));
}

TEST_F(PaginationDescriptorTest, AssignMapPaginationMethodVars) {
  VarsDictionary method_vars;
  auto const* file_descriptor =
      pool_.FindFileByName("google/foo/v1/service.proto");
  AssignPaginationMethodVars(*file_descriptor->service(0)->method(0),
                             method_vars);
  EXPECT_THAT(method_vars["range_output_type"],
              Eq("std::pair<std::string, test::Foo>"));
  EXPECT_THAT(
      method_vars["method_paginated_return_doxygen_link"],
      Eq("@googleapis_link{test::Foo,google/foo/v1/service.proto#L12}"));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
