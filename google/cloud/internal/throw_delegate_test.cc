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

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/testing_util/expect_exception.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

std::string const kCmsg("testing with std::string const&");
char const* msg = "testing with char const*";
using ::testing::HasSubstr;

TEST(ThrowDelegateTest, InvalidArgument) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ThrowInvalidArgument(msg), std::invalid_argument);
  EXPECT_THROW(ThrowInvalidArgument(kCmsg), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ThrowInvalidArgument(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(ThrowInvalidArgument(kCmsg), kCmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, RangeError) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ThrowRangeError(msg), std::range_error);
  EXPECT_THROW(ThrowRangeError(kCmsg), std::range_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(ThrowRangeError(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(ThrowRangeError(kCmsg), kCmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, RuntimeError) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ThrowRuntimeError(msg), std::runtime_error);
  EXPECT_THROW(ThrowRuntimeError(kCmsg), std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(ThrowRuntimeError(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(ThrowRuntimeError(kCmsg), kCmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, SystemError) {
  auto ec = std::make_error_code(std::errc::bad_message);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { ThrowSystemError(ec, msg); } catch (std::system_error const& ex) {
        EXPECT_EQ(ec, ex.code());
        EXPECT_THAT(ex.what(), HasSubstr(msg));
        throw;
      },
      std::system_error);

  EXPECT_THROW(
      try { ThrowSystemError(ec, kCmsg); } catch (std::system_error const& ex) {
        EXPECT_EQ(ec, ex.code());
        EXPECT_THAT(ex.what(), HasSubstr(kCmsg));
        throw;
      },
      std::system_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(ThrowSystemError(ec, msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(ThrowSystemError(ec, kCmsg), kCmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, LogicError) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ThrowLogicError(msg), std::logic_error);
  EXPECT_THROW(ThrowLogicError(kCmsg), std::logic_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(ThrowLogicError(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(ThrowLogicError(kCmsg), kCmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, TestThrow) {
  testing_util::ExpectException<RuntimeStatusError>(
      [&] {
        Status status(StatusCode::kNotFound, "NOT FOUND");
        ThrowStatus(std::move(status));
      },
      [&](RuntimeStatusError const& ex) {
        EXPECT_EQ(StatusCode::kNotFound, ex.status().code());
        EXPECT_EQ("NOT FOUND", ex.status().message());
      },
      "Aborting because exceptions are disabled: "
      "NOT FOUND \\[NOT_FOUND\\]");
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
