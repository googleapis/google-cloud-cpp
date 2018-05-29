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
#include <gtest/gtest.h>

using namespace google::cloud::internal;

namespace {
std::string const cmsg("testing with std::string const&");
char const* msg = "testing with char const*";
}  // namespace

TEST(ThrowDelegateTest, InvalidArgument) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(RaiseInvalidArgument(msg), std::invalid_argument);
  EXPECT_THROW(RaiseInvalidArgument(cmsg), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(RaiseInvalidArgument(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(RaiseInvalidArgument(cmsg), cmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, RangeError) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(RaiseRangeError(msg), std::range_error);
  EXPECT_THROW(RaiseRangeError(cmsg), std::range_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(RaiseRangeError(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(RaiseRangeError(cmsg), cmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, RuntimeError) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(RaiseRuntimeError(msg), std::runtime_error);
  EXPECT_THROW(RaiseRuntimeError(cmsg), std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(RaiseRuntimeError(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(RaiseRuntimeError(cmsg), cmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ThrowDelegateTest, LogicError) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(RaiseLogicError(msg), std::logic_error);
  EXPECT_THROW(RaiseLogicError(cmsg), std::logic_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(RaiseLogicError(msg), msg);
  EXPECT_DEATH_IF_SUPPORTED(RaiseLogicError(cmsg), cmsg);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
