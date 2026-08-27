// Copyright 2022 Google LLC
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

#include "google/cloud/internal/debug_string_protobuf.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/tracing_options.h"
#include "google/iam/v1/policy.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

google::iam::v1::Policy MakePolicy() {
  auto constexpr kText = R"pb(
    bindings {
      role: "roles/viewer"
      members: "user:user1@example.com"
      members: "user:user2@example.com"
    }
    audit_configs {
      audit_log_configs {
        exempted_members: "user:user3@example.com"
        exempted_members: "user:user4@example.com"
      }
    }
  )pb";
  google::iam::v1::Policy policy;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &policy));
  return policy;
}

TEST(LogWrapperHelpers, DefaultOptions) {
  TracingOptions tracing_options;
  // clang-format off
  std::string const text =
    R"pb(google.iam.v1.Policy { )pb"
    R"pb(goo.gle/debugonly )pb"
    R"pb(bindings { )pb"
    R"pb(role: "roles/viewer" )pb"
    R"pb(members: "user:user1@example.com" )pb"
    R"pb(members: "user:user2@example.com" )pb"
    R"pb(} )pb"
    R"pb(audit_configs { )pb"
    R"pb(audit_log_configs { )pb"
    R"pb(exempted_members: "user:user3@example.com" )pb"
    R"pb(exempted_members: "user:user4@example.com" )pb"
    R"pb(} )pb"
    R"pb(} )pb"
    R"pb(})pb";
  // clang-format on
  EXPECT_EQ(text, DebugString(MakePolicy(), tracing_options));
}

TEST(LogWrapperHelpers, MultiLine) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("single_line_mode=off");
  // clang-format off
  std::string const text = R"pb(google.iam.v1.Policy {
  goo.gle/debugonly 
  bindings {
    role: "roles/viewer"
    members: "user:user1@example.com"
    members: "user:user2@example.com"
  }
  audit_configs {
    audit_log_configs {
      exempted_members: "user:user3@example.com"
      exempted_members: "user:user4@example.com"
    }
  }
})pb";
  // clang-format on
  EXPECT_EQ(text, DebugString(MakePolicy(), tracing_options));
}

TEST(LogWrapperHelpers, Truncate) {
  TracingOptions tracing_options;
  tracing_options.SetOptions("truncate_string_field_longer_than=8");
  // clang-format off
  std::string const text =
    R"pb(google.iam.v1.Policy { )pb"
    R"pb(goo.gle/debugonly )pb"
    R"pb(bindings { )pb"
    R"pb(role: "roles/vi...<truncated>..." )pb"
    R"pb(members: "user:use...<truncated>..." )pb"
    R"pb(members: "user:use...<truncated>..." )pb"
    R"pb(} )pb"
    R"pb(audit_configs { )pb"
    R"pb(audit_log_configs { )pb"
    R"pb(exempted_members: "user:use...<truncated>..." )pb"
    R"pb(exempted_members: "user:use...<truncated>..." )pb"
    R"pb(} )pb"
    R"pb(} )pb"
    R"pb(})pb";
  // clang-format on
  EXPECT_EQ(text, DebugString(MakePolicy(), tracing_options));
}

TEST(LogWrapperHelpers, Duration) {
  google::protobuf::Duration duration;
  duration.set_seconds((11 * 60 + 22) * 60 + 33);
  duration.set_nanos(123456789);
  std::string const expected =
      R"(google.protobuf.Duration { goo.gle/debugonly "11h22m33.123456789s" })";
  EXPECT_EQ(expected, DebugString(duration, TracingOptions{}.SetOptions(
                                                "single_line_mode=on")));
}

TEST(LogWrapperHelpers, Timestamp) {
  google::protobuf::Timestamp timestamp;
  timestamp.set_seconds(1658470436);
  timestamp.set_nanos(123456789);
  std::string const expected = R"(google.protobuf.Timestamp {
  goo.gle/debugonly 
  "2022-07-22T06:13:56.123456789Z"
})";
  EXPECT_EQ(expected, DebugString(timestamp, TracingOptions{}.SetOptions(
                                                 "single_line_mode=off")));
}

TEST(LogWrapperHelpers, RedactedField) {
  google::protobuf::FileDescriptorProto file_proto;
  file_proto.set_name("test_redact.proto");
  file_proto.set_syntax("proto3");
  google::protobuf::DescriptorProto* message_proto =
      file_proto.add_message_type();
  message_proto->set_name("RedactedMessage");

  google::protobuf::FieldDescriptorProto* unredacted_field =
      message_proto->add_field();
  unredacted_field->set_name("public_field");
  unredacted_field->set_number(1);
  unredacted_field->set_type(google::protobuf::FieldDescriptorProto::TYPE_STRING);

  google::protobuf::FieldDescriptorProto* redacted_field =
      message_proto->add_field();
  redacted_field->set_name("secret_key");
  redacted_field->set_number(2);
  redacted_field->set_type(google::protobuf::FieldDescriptorProto::TYPE_STRING);
  redacted_field->mutable_options()->set_debug_redact(true);

  google::protobuf::DescriptorPool pool;
  google::protobuf::FileDescriptor const* file_desc =
      pool.BuildFile(file_proto);
  ASSERT_THAT(file_desc, ::testing::NotNull());
  google::protobuf::Descriptor const* msg_desc =
      file_desc->FindMessageTypeByName("RedactedMessage");
  ASSERT_THAT(msg_desc, ::testing::NotNull());

  google::protobuf::DynamicMessageFactory factory;
  std::unique_ptr<google::protobuf::Message> message(
      factory.GetPrototype(msg_desc)->New());
  google::protobuf::Reflection const* reflection = message->GetReflection();
  reflection->SetString(message.get(), msg_desc->field(0), "public_value");
  reflection->SetString(message.get(), msg_desc->field(1), "secret_value");

  TracingOptions tracing_options;
  std::string const actual = DebugString(*message, tracing_options);
  EXPECT_THAT(actual, ::testing::HasSubstr("public_value"));
  EXPECT_THAT(actual, ::testing::Not(::testing::HasSubstr("secret_value")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
